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

#ifndef H_RATIONAL
#define H_RATIONAL

#define     Abs(v)  (((v)<0) ? -(v) : (v))

// enforce protection vs divide by 0
// shall eventually be removed when code is matured
#define PROTECTDIVIDEBYZERO 1

#undef TRACE_TAG
#define TRACE_TAG "Rational_c"

class Rational_c
{
public:
    Rational_c(void)
        : Negative(false)
        , Numerator(0)
        , Denominator(1)
    {}

    Rational_c(long long   N)
        : Negative(N < 0)
        , Numerator(Abs(N))
        , Denominator(1)
    {}

    Rational_c(long long   N,
               long long   D)
        : Negative((N < 0) ^ (D < 0))
        , Numerator(Abs(N))
        , Denominator(Abs(D))
    {
#if PROTECTDIVIDEBYZERO
        if (0 == Denominator)
        {
            SE_WARNING("ctor2 den0\n");
            Denominator = 1;
        }
#endif
    }

    Rational_c(unsigned long long N,
               unsigned long long D,
               unsigned long long R,
               bool               Neg)

        : Negative(Neg)
        , Numerator(N)
        , Denominator(D)
    {
#if PROTECTDIVIDEBYZERO
        if (0 == Denominator)
        {
            SE_WARNING("ctor3 den0\n");
            Denominator = 1;
        }
#endif

        //
        // This is a cheat, whenever we do a calculation, we often
        // have a value that may divide both top and bottom.
        // Here we check it (note I provided three as a default possible.
        //

        if (R &&
            (R * (Numerator / R) == Numerator) &&
            (R * (Denominator / R) == Denominator))
        {
            Numerator   = Numerator / R;
            Denominator = Denominator / R;
        }
    }

    // ////////////////////////////////////////////////////////////////////////
    //
    // Restriction function, brings the numerator and
    // denominator down to 32 bit values.
    //

    void   Restrict(void)
    {
        unsigned long long  Tmp;
        unsigned int        Shift       = 0;
        unsigned int        SpareBitsForArithmetic;
        unsigned long long  NewDenominator;

        if (Numerator == 0)
        {
            Negative  = false;
        }

#if PROTECTDIVIDEBYZERO
        if (0 == Denominator)
        {
            SE_WARNING("R den0\n");
            Denominator = 1;
        }
#endif

        // Check if restriction could be ignored
        if (!unlikely(Numerator & 0xffffffff00000000ULL) && !unlikely(Denominator & 0xffffffff00000000ULL))
        {
            return;
        }

        // Try to bring the rational down before restricting it
        while ((Numerator != 0) && (Denominator != 0) && ((Numerator & 0x01) == 0) && ((Denominator & 0x01) == 0))
        {
            Numerator >>= 1;
            Denominator >>= 1;

            if (!unlikely(Denominator & 0xffffffff00000000ULL) && !unlikely(Denominator & 0xffffffff00000000ULL))
            {
                return;
            }
        }

        Tmp = (Numerator | Denominator) >> 32;

        if (Tmp != 0)
        {
            Shift   += ((Tmp & 0xffff0000) != 0) ? 16 : 0;
            Shift   += (((Tmp >> Shift) & 0x0000ff00) != 0) ? 8 : 0;
            Shift   += (((Tmp >> Shift) & 0x000000f0) != 0) ? 4 : 0;
            Shift   += (((Tmp >> Shift) & 0x0000000c) != 0) ? 2 : 0;
            Shift   += (((Tmp >> Shift) & 0x00000002) != 0) ? 1 : 0;
            //
            // We know how much we are going to shift, however we have a problem
            // if the denominator is small and the numerator is large, we may well
            // seriously damage the accuracy, so, if it is possible we scale both
            // numerator and the denominator so that the shift will not hurt the
            // accuracy (I know this sounds highly dubious, but it appears to work
            // so there).
            //
            SpareBitsForArithmetic  = 31 - Shift;       // Careful we do not overflow doing this scale,

            if (Denominator < (1ULL << SpareBitsForArithmetic))
            {
                NewDenominator  = Denominator & (0xffffffffULL << (Shift + 1));
                Numerator   = (Numerator * NewDenominator) / Denominator;
                Denominator = NewDenominator;
            }

            Numerator       = (Numerator + (1ULL << Shift)) >> (Shift + 1);
            Denominator     = (Denominator + (1ULL << Shift)) >> (Shift + 1);
        }

        //
        // Empirically I noticed three oft repeated conditions
        // which cause me to shift, and lose some accuracy.
        // The first two are when one of the denominator, or
        // numerator is a multiple of the other, the third
        // involves the use of factors of 10, I have eliminated
        // both cases, and reduced the accuracy losing shift a
        // great deal.
        //
        if (0 == Denominator)
        {
            SE_WARNING("Restrict leading to den0\n");
            Denominator = 1;
        }

        if ((Denominator != 1) && (Numerator != 1))
        {
            if ((Denominator * (Numerator / Denominator)) == Numerator)
            {
                Numerator       = Numerator / Denominator;
                Denominator     = 1;
            }
            else if ((Numerator * (Denominator / Numerator)) == Denominator)
            {
                Denominator     = Denominator / Numerator;
                Numerator       = Numerator / Numerator;
            }
            else while (((10 * (Numerator / 10)) == Numerator) &&
                            ((10 * (Denominator / 10)) == Denominator))
                {
                    Numerator       = Numerator / 10;
                    Denominator     = Denominator / 10;
                }
        }

        // Num/Den values supposed to be down to 32 bit values
        SE_ASSERT(!unlikely(Numerator & 0xffffffff00000000ULL) && !unlikely(Denominator & 0xffffffff00000000ULL));
    }

    // ////////////////////////////////////////////////////////////////////////
    //
    // Truncate function - restrict the denominator and truncate
    //

    Rational_c  TruncateDenominator(unsigned int    NewDenominator)
    {
#if PROTECTDIVIDEBYZERO
        if (0 == Denominator)
        {
            SE_WARNING("TD den0\n");
            Denominator = 1;
        }
#endif
        return Rational_c((NewDenominator * Numerator) / Denominator, NewDenominator, 3, Negative);
    }

    // ////////////////////////////////////////////////////////////////////////
    //
    // Assignment functions
    //

    Rational_c &operator= (int  V)
    {
        Negative    = V < 0;
        Numerator   = Abs(V);
        Denominator = 1;
        return *this;
    }

    Rational_c &operator= (Rational_c   R)
    {
        Negative    = R.Negative;
        Numerator   = R.Numerator;
        Denominator = R.Denominator;
#if PROTECTDIVIDEBYZERO
        if (0 == Denominator)
        {
            SE_WARNING("den0\n");
            Denominator = 1;
        }
#endif
        Restrict();
        return *this;
    }

    // ////////////////////////////////////////////////////////////////////////
    //
    // Comparison
    //

    bool   operator== (Rational_c   F)
    {
        return ((Negative == F.Negative) && ((Numerator * F.Denominator) == (F.Numerator * Denominator)));
    }

    bool   operator== (int  I)
    {
        return (Negative == (I < 0)) && (Numerator == (Abs(I) * Denominator));
    }

    bool   operator!= (Rational_c   F)
    {
        return ((Negative != F.Negative) || ((Numerator * F.Denominator) != (F.Numerator * Denominator)));
    }

    bool   operator!= (int  I)
    {
        return (Negative != (I < 0)) || (Numerator != (Abs(I) * Denominator));
    }

    bool   operator>= (Rational_c   F)
    {
        return  Negative ?
                (F.Negative ?
                 ((Numerator * F.Denominator) <= (F.Numerator * Denominator)) :
                 ((Numerator == 0) && (F.Numerator == 0))) :
                (F.Negative ?
                 (true) :
                 (Numerator * F.Denominator) >= (F.Numerator * Denominator));
    }

    bool   operator>= (int  I)
    {
        Rational_c  Temp = I;
        return *this >= Temp;
    }

    bool   operator> (Rational_c    F)
    {
        return  Negative ?
                (F.Negative ?
                 ((Numerator * F.Denominator) < (F.Numerator * Denominator)) :
                 (false)) :
                (F.Negative ?
                 ((Numerator == 0) && (F.Numerator == 0)) :
                 (Numerator * F.Denominator) > (F.Numerator * Denominator));
    }

    bool   operator> (int   I)
    {
        Rational_c  Temp = I;
        return *this > Temp;
    }

    bool   operator<= (Rational_c   F)
    {
        return !(*this > F);
    }

    bool   operator<= (int  I)
    {
        return !(*this > I);
    }

    bool   operator< (Rational_c    F)
    {
        return !(*this >= F);
    }

    bool   operator< (int   I)
    {
        return !(*this >= I);
    }

    // ////////////////////////////////////////////////////////////////////////
    //
    // Add and Subtract
    //

    Rational_c   operator+ (Rational_c  F)
    {
        if (Negative ^ F.Negative)
        {
            bool    This_GT_F   = (Numerator * F.Denominator) > (F.Numerator * Denominator);

            if (This_GT_F)
            {
                return Rational_c((Numerator * F.Denominator) - (F.Numerator * Denominator),
                                  Denominator * F.Denominator,
                                  Denominator,
                                  Negative);
            }
            else
            {
                return Rational_c((F.Numerator * Denominator) - (Numerator * F.Denominator),
                                  Denominator * F.Denominator,
                                  Denominator,
                                  !Negative);
            }
        }
        else
        {
            return Rational_c((Numerator * F.Denominator) + (F.Numerator * Denominator),
                              Denominator * F.Denominator,
                              Denominator,
                              Negative);
        }
    }

    Rational_c   operator+= (Rational_c F)
    {
        return (*this = *this + F);
    }

    Rational_c   operator+= (int    I)
    {
        return (*this = *this + I);
    }

    Rational_c   operator- (Rational_c  F)
    {
        if (Negative ^ F.Negative)
        {
            return Rational_c((Numerator * F.Denominator) + (F.Numerator * Denominator),
                              Denominator * F.Denominator,
                              Denominator,
                              Negative);
        }
        else
        {
            bool    This_GT_F   = (Numerator * F.Denominator) > (F.Numerator * Denominator);

            if (This_GT_F)
            {
                return Rational_c((Numerator * F.Denominator) - (F.Numerator * Denominator),
                                  Denominator * F.Denominator,
                                  Denominator,
                                  Negative);
            }
            else
            {
                return Rational_c((F.Numerator * Denominator) - (Numerator * F.Denominator),
                                  Denominator * F.Denominator,
                                  Denominator,
                                  !Negative);
            }
        }
    }

    Rational_c   operator- (int I)
    {
        Rational_c      Result = *this;
        unsigned long long  AbsI = Abs(I);

        if ((I < 0) != Negative)
        {
            Result.Numerator  += (AbsI * Denominator);
        }
        else if ((AbsI * Denominator) <= Numerator)
        {
            Result.Numerator  -= (AbsI * Denominator);
        }
        else
        {
            Result.Numerator     = (AbsI * Denominator) - Numerator;
            Result.Negative  = !Negative;
        }

        return Result;
    }

    Rational_c   operator-= (Rational_c F)
    {
        return (*this = *this - F);
    }

    Rational_c   operator-= (int    I)
    {
        return (*this = *this - I);
    }

    // ////////////////////////////////////////////////////////////////////////
    //
    // Multiply
    //

    Rational_c   operator* (Rational_c  F)
    {
        return Rational_c(Numerator * F.Numerator, Denominator * F.Denominator, min(Denominator, F.Denominator), (Negative ^ F.Negative));
    }

    Rational_c   operator* (int V)
    {
        return Rational_c(Numerator * Abs(V), Denominator, min(Denominator, (unsigned long long) Abs(V)), (V < 0) ^ Negative);
    }

    Rational_c   operator*= (Rational_c F)
    {
        return (*this = *this * F);
    }

    Rational_c   operator*= (int    V)
    {
        return (*this = *this * V);
    }


    // ////////////////////////////////////////////////////////////////////////
    //
    // Divide
    //

    Rational_c   operator/ (Rational_c  F)
    {
        long long Factor    = 10;

        if (Numerator == F.Numerator)
        {
            Factor    = Numerator;
        }
        else if (Denominator == F.Denominator)
        {
            Factor    = Denominator;
        }

#if PROTECTDIVIDEBYZERO
        if (0 == F.Numerator)
        {
            SE_WARNING("op/ with num0\n");
            F.Numerator = 1;

        }
#endif
        return Rational_c(Numerator * F.Denominator, Denominator * F.Numerator, Factor, (Negative ^ F.Negative));
    }

    Rational_c   operator/ (int V)
    {
#if PROTECTDIVIDEBYZERO

        if (0 == V)
        {
            SE_WARNING("op/ with V0\n");
            V = 1;
        }

#endif
        return Rational_c(Numerator, Denominator * Abs(V), Abs(V), (V < 0) ^ Negative);
    }

    Rational_c   operator/= (Rational_c F)
    {
        return (*this = *this / F);
    }

    Rational_c   operator/= (int    V)
    {
        return (*this = *this / V);
    }


    // ////////////////////////////////////////////////////////////////////////
    //
    // Extraction
    //

    long long   GetNumerator(void)
    {
        return Negative ? -Numerator : Numerator;
    }

    long long   GetDenominator(void)
    {
#if PROTECTDIVIDEBYZERO
        if (0 == Denominator)
        {
            SE_WARNING("GD den0\n");
            Denominator = 1;
        }
#endif
        return Denominator;
    }

    long long   LongLongIntegerPart(void)
    {
#if PROTECTDIVIDEBYZERO
        if (0 == Denominator)
        {
            SE_WARNING("LLIP den0\n");
            Denominator = 1;
        }
#endif
        // treat the case where the Denominator exceeding 32bits
        if (unlikely(Denominator & 0xffffffff00000000ULL))
        {
            Rational_c RestrictedCopy = *this;
            // Restrict the rational to avoid the case where denominator exceeding 32bits
            RestrictedCopy.Restrict();
            // Check LongLongIntegerPart with multiply
            long long InitialIntegerPartGuess = RestrictedCopy.LongLongIntegerPart();

            while ((InitialIntegerPartGuess * Denominator) > (Negative ? -(Numerator) : Numerator))
            {
                InitialIntegerPartGuess--;
            }

            return InitialIntegerPartGuess;
        }
        else
        {
            return (Negative ? -(Numerator / Denominator) : (Numerator / Denominator));
        }
    }

    int   IntegerPart(void)
    {
        // treat the case where the Denominator exceeding 32bits
        if (unlikely(Denominator & 0xffffffff00000000ULL))
        {
            Rational_c RestrictedCopy = *this;
            // Restrict the rational to avoid the case where denominator exceeding 32bits
            RestrictedCopy.Restrict();
            // Check IntegerPart with multiply
            int InitialIntegerPartGuess = RestrictedCopy.IntegerPart();

            while ((InitialIntegerPartGuess * Denominator) > (Negative ? -(Numerator) : Numerator))
            {
                InitialIntegerPartGuess--;
            }

            return InitialIntegerPartGuess;
        }
        else
        {
#if PROTECTDIVIDEBYZERO
            if (0 == Denominator)
            {
                SE_WARNING("IP den0\n");
                Denominator = 1;
            }
#endif
            return (int)(Negative ? -(Numerator / Denominator) : (Numerator / Denominator));
        }
    }

    long long   RoundedLongLongIntegerPart(void)
    {
        // treat the case where the Denominator exceeding 32bits
        if (unlikely(Denominator & 0xffffffff00000000ULL))
        {
            Rational_c RestrictedCopy = *this;
            // Restrict the rational to avoid the case where denominator exceeding 32bits
            RestrictedCopy.Restrict();
            // Check RoundedLongLongIntegerPart with multiply
            long long InitialIntegerPartGuess = RestrictedCopy.RoundedLongLongIntegerPart();
            while ((InitialIntegerPartGuess * Denominator) > (Negative ? -(Numerator + (Denominator / 2)) : (Numerator + (Denominator / 2))))
            {
                InitialIntegerPartGuess--;
            }

            return InitialIntegerPartGuess;
        }
        else
        {
#if PROTECTDIVIDEBYZERO
            if (0 == Denominator)
            {
                SE_WARNING("RLLIP den0\n");
                Denominator = 1;
            }
#endif
            return Negative ?
                   -((Numerator + (Denominator / 2)) / Denominator) :
                   ((Numerator + (Denominator / 2)) / Denominator);
        }
    }

    int   RoundedIntegerPart(void)
    {
        // treat the case where the Denominator exceeding 32bits
        if (unlikely(Denominator & 0xffffffff00000000ULL))
        {
            Rational_c RestrictedCopy = *this;

            // Restrict the rational to avoid the case where denominator exceeding 32bits
            RestrictedCopy.Restrict();
            // Check RoundedIntegerPart with multiply
            int InitialIntegerPartGuess = RestrictedCopy.RoundedIntegerPart();
            while ((InitialIntegerPartGuess * Denominator) > (Negative ? -(Numerator + (Denominator / 2)) : (Numerator + (Denominator / 2))))
            {
                InitialIntegerPartGuess--;
            }

            return InitialIntegerPartGuess;
        }
        else
        {

#if PROTECTDIVIDEBYZERO
            if (0 == Denominator)
            {
                SE_WARNING("RIP den0\n");
                Denominator = 1;
            }
#endif
            return (Negative > 0) ?
                   -(int)((Numerator + (Denominator / 2)) / Denominator) :
                   (int)((Numerator + (Denominator / 2)) / Denominator);
        }
    }

    long long   RoundedUpLongLongIntegerPart(void)
    {
        // treat the case where the Denominator exceeding 32bits
        if (unlikely(Denominator & 0xffffffff00000000ULL))
        {
            Rational_c RestrictedCopy = *this;

            // Restrict the rational to avoid the case where denominator exceeding 32bits
            RestrictedCopy.Restrict();
            // Check RoundedUpLongLongIntegerPart with multiply
            long long InitialIntegerPartGuess = RestrictedCopy.RoundedUpLongLongIntegerPart();
            while ((InitialIntegerPartGuess * Denominator) > (Negative ? -(Numerator + (Denominator - 1)) : (Numerator + (Denominator - 1))))
            {
                InitialIntegerPartGuess--;
            }

            return InitialIntegerPartGuess;
        }
        else
        {
#if PROTECTDIVIDEBYZERO
            if (0 == Denominator)
            {
                SE_WARNING("RULLIP den0\n");
                Denominator = 1;
            }
#endif
            return Negative ?
                   -((Numerator + (Denominator - 1)) / Denominator) :
                   ((Numerator + (Denominator - 1)) / Denominator);
        }
    }

    int   RoundedUpIntegerPart(void)
    {
        // treat the case where the Denominator exceeding 32bits
        if (unlikely(Denominator & 0xffffffff00000000ULL))
        {
            Rational_c RestrictedCopy = *this;

            // Restrict the rational to avoid the case where denominator exceeding 32bits
            RestrictedCopy.Restrict();
            // Check RoundedUpIntegerPart with multiply
            int InitialIntegerPartGuess = RestrictedCopy.RoundedUpIntegerPart();

            while ((InitialIntegerPartGuess * Denominator) > (Negative ? -(Numerator + (Denominator - 1)) : (Numerator + (Denominator - 1))))
            {
                InitialIntegerPartGuess--;
            }

            return InitialIntegerPartGuess;
        }
        else
        {
#if PROTECTDIVIDEBYZERO
            if (0 == Denominator)
            {
                SE_WARNING("RUIP den0\n");
                Denominator = 1;
            }
#endif
            return Negative ?
                   -(int)((Numerator + (Denominator - 1)) / Denominator) :
                   (int)((Numerator + (Denominator - 1)) / Denominator);
        }
    }

    Rational_c   Remainder(void)
    {
#if PROTECTDIVIDEBYZERO
        if (0 == Denominator)
        {
            SE_WARNING("R den0\n");
            Denominator = 1;
        }
#endif
        // return Rational_c( Numerator % Denominator, Denominator, 2, Negative );
        // mod does not work in the kernel for 64 bit numbers
        // here we use IntegerPart() instead of Numerator/Denominator to treat the case where Denominator exceeding 32bits
        return Rational_c(Numerator - (Denominator * this->IntegerPart()), Denominator, 3, Negative);
    }

    int   RemainderDecimal(unsigned int Places = 6)
    {
        Rational_c  Tmp = Remainder();
#if PROTECTDIVIDEBYZERO
        if (0 == Denominator)
        {
            SE_WARNING("RD den0\n");
            Denominator = 1;
        }
#endif

        switch (Places)
        {
        case    3:  Tmp.Numerator   *= 1000;                break;

        case    6:  Tmp.Numerator   *= 1000000;             break;

        default:
        case    9:  Tmp.Numerator   *= 1000000000;              break;
        }

        return Tmp.Numerator / Tmp.Denominator;
    }

    // ////////////////////////////////////////////////////////////////////////
    //
    // Printing
    //

    void   DumpVal(int trgroup, const char *S = "")
    {
        SE_INFO(trgroup, "%s{%d %016llx, %016llx}\n", S, Negative, Numerator, Denominator);
    }

private:
    bool            Negative;
    unsigned long long  Numerator;
    unsigned long long  Denominator;
};

typedef Rational_c Rational_t;

// //////////////////////////////////////////////////////////////////////////////////////////
//
//  The inconvenient bunch where the non rational is on the left
//

static inline Rational_c   operator+ (long long        I,
                                      Rational_c  F)
{
    return F + I;
}

static inline Rational_c   operator- (long long        I,
                                      Rational_c  F)
{
    Rational_c  Temp = I;
    return Temp - F;
}

static inline Rational_c   operator* (long long        V,
                                      Rational_c  F)
{
    return F * V;
}

static inline Rational_c   operator/ (long long       V,
                                      Rational_c  F)
{
    Rational_c  Temp = V;
    return Temp / F;
}


// //////////////////////////////////////////////////////////////////////////////////////////
//
//  Supplying the useful extraction bits as standalone functions
//

static inline long long   LongLongIntegerPart(Rational_c    R)
{
    return R.LongLongIntegerPart();
}

static inline int   IntegerPart(Rational_c  R)
{
    return R.IntegerPart();
}

static inline long long   RoundedLongLongIntegerPart(Rational_c R)
{
    return R.RoundedLongLongIntegerPart();
}

static inline int   RoundedIntegerPart(Rational_c   R)
{
    return R.RoundedIntegerPart();
}

static inline Rational_c   Remainder(Rational_c R)
{
    return R.Remainder();
}

static inline int   RemainderDecimal(Rational_c R)
{
    return R.RemainderDecimal();
}

static inline Rational_c   TruncateDenominator(Rational_c   R,
                                               unsigned int    N)
{
    return R.TruncateDenominator(N);
}

#endif
