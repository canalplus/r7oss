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

Source file name : longlonglong.h
Author :           Nick

Definition of the class supporting 96 bit integers for use in the least squares class


Date        Modification                                    Name
----        ------------                                    --------
18-Mar-09   Created                                         Nick

************************************************************************/


#ifndef H_LONGLONGLONG
#define H_LONGLONGLONG

#ifndef Abs
#define 	Abs(v)	(((v)<0) ? -(v) : (v))
#endif

// -------------------------------------------------------------

class LongLongLong_c
{
private:
	bool			Negative;
	unsigned long long	Upper64;
	unsigned long long	Lower32;

public:

    LongLongLong_c( long long	Value = 0 )
    {
	Negative	= Value < 0;
	Upper64	= Abs(Value) >> 32;
	Lower32	= Abs(Value) & 0xffffffff;
    }

    // ////////////////////////////////////////////////////////////////////////
    //
    // Assignment functions
    //

    bool   operator= (	long long 	V )
    {
	Negative	= V < 0;
	Upper64		= Abs(V) >> 32;
	Lower32		= Abs(V) & 0xffffffff;
	return true;
    }

//

    bool   operator= (	LongLongLong_c	R )
    {
	Negative	= R.Negative;
	Upper64		= R.Upper64;
	Lower32		= R.Lower32;
	return true;
    }

    // ////////////////////////////////////////////////////////////////////////
    //
    // Add and Subtract
    //

    LongLongLong_c   operator+ (	LongLongLong_c	F )
    {
	LongLongLong_c	Temp;
	LongLongLong_c	Result;

	if( Negative != F.Negative )
	{
	    Temp		= F;
	    Temp.Negative 	= Negative;
	    return (*this - Temp);
	}

	Result.Negative		= Negative;
	Result.Lower32		= Lower32 + F.Lower32;
	Result.Upper64		= Upper64 + F.Upper64;
	Result.Upper64		+= (Result.Lower32 >> 32);
	Result.Lower32		&= 0xffffffff;

	return Result;
    }

//

    LongLongLong_c   operator+ (	long long	I )
    {
	LongLongLong_c	Temp( I );
	return (*this + Temp);
    }

//

    LongLongLong_c   operator+= (	LongLongLong_c	F )
    {
    	return (*this = *this + F);
    }

//

    LongLongLong_c   operator+= (	long long	I )
    {
	return (*this = *this + I);
    }
    
//

    LongLongLong_c   operator- (	LongLongLong_c	F )
    {
	LongLongLong_c	Temp;
	LongLongLong_c	Result;

	if( Negative != F.Negative )
	{
	    Temp		= F;
	    Temp.Negative 	= Negative;
	    return (*this + Temp);
	}

	if(  (Upper64 > F.Upper64) ||
	    ((Upper64 == F.Upper64) && (Lower32 > F.Lower32)) )
	{
	    Result.Negative	= Negative;
	    Result.Lower32	= Lower32 - F.Lower32;
	    Result.Upper64	= Upper64 - F.Upper64;
	    if( Result.Lower32 > 0xffffffff )
	    {
	    	Result.Lower32	+= 0x100000000ull;
	    	Result.Upper64	-= 1;
	    }
	}
	else
	{
	    Result.Negative	= !Negative;
	    Result.Lower32	= F.Lower32 - Lower32;
	    Result.Upper64	= F.Upper64 - Upper64;
	    if( Result.Lower32 > 0xffffffff )
	    {
	    	Result.Lower32	+= 0x100000000ull;
	    	Result.Upper64	-= 1;
	    }
	}
	return Result;
    }

//

    LongLongLong_c   operator- (	long long	I )
    {
	LongLongLong_c	Temp( I );
	return (*this - Temp);
    }

//
    LongLongLong_c   operator-= (	LongLongLong_c	F )
    {
	return (*this = *this - F);
    }

//

    LongLongLong_c   operator-= (	long long	I )
    {
	return (*this = *this - I);
    }

    // ////////////////////////////////////////////////////////////////////////
    //
    // Multiply
    //

    LongLongLong_c   operator* (	LongLongLong_c	F )
    {
	LongLongLong_c	Product;

	Product.Negative	= (Negative ^ F.Negative);
	Product.Lower32		= Lower32 * F.Lower32;
	Product.Upper64		= (Lower32 * F.Upper64) + (Upper64 * F.Lower32) + ((Upper64 * F.Upper64) << 32);
	Product.Upper64		+= (Product.Lower32 >> 32);
	Product.Lower32		&= 0xffffffff;
	
	return Product;
    }

//

    LongLongLong_c   operator* (	long long	V )
    {
	LongLongLong_c	Temp( V );
	return (*this * Temp);
    }

    // ////////////////////////////////////////////////////////////////////////
    //
    // Extraction
    //

    void   Get( 	long long 	*Value,
			unsigned int	*Shifted )
    {
	unsigned int 	UpperBits	= 0;
	long long	Val		= 0;
	unsigned int	Shift		= 0;

	if( Upper64 != 0 )
	{
	    UpperBits	+= ((Upper64 & 0xffffffff00000000ull) != 0) ? 32 : 0;
	    UpperBits	+= (((Upper64 >> UpperBits) & 0xffff0000) != 0) ? 16 : 0;
	    UpperBits	+= (((Upper64 >> UpperBits) & 0x0000ff00) != 0) ? 8 : 0;
	    UpperBits	+= (((Upper64 >> UpperBits) & 0x000000f0) != 0) ? 4 : 0;
	    UpperBits	+= (((Upper64 >> UpperBits) & 0x0000000c) != 0) ? 2 : 0;
	    UpperBits	+= (((Upper64 >> UpperBits) & 0x00000002) != 0) ? 1 : 0;
	    UpperBits++;
	}

	Shift	= (UpperBits > 31) ? (UpperBits - 31) : 0;
	Val	= ((Shift > 31) ? (Upper64 >> (Shift - 32)) : (Upper64 << (32 - Shift))) | (Lower32 >> Shift);

	*Value	= Negative ? -Val : Val;
	*Shifted= Shift;
    }
};

typedef LongLongLong_c LongLongLong_t;

// //////////////////////////////////////////////////////////////////////////////////////////
//
//	The inconvenient bunch where the non long long long is on the left
//

static inline LongLongLong_c   operator+ (	long long	I,
						LongLongLong_c	F )
{
    LongLongLong_c	Temp = I;
    return Temp + F;
}

//

static inline LongLongLong_c   operator- (	long long	I,
						LongLongLong_c	F )
{
    LongLongLong_c	Temp = I;
    return Temp - F;
}

//

static inline LongLongLong_c   operator* (	long long	V,
						LongLongLong_c	F )
{
    LongLongLong_c	Temp = V;
    return Temp * F;
}

//

#endif

