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

#include "timestamps.h"

TimeStamp_c::TimeStamp_c(void)
    : StoredValue(UNSPECIFIED_TIME)
    , StoredFormat(TIME_FORMAT_US)
{}

TimeStamp_c::~TimeStamp_c(void)
{
}

TimeStamp_c::TimeStamp_c(int64_t ValueIn, stm_se_time_format_t FormatIn)
    : StoredValue(ValueIn)
    , StoredFormat(FormatIn)
{}

TimeStamp_c TimeStamp_c::Set(int64_t ValueIn, stm_se_time_format_t FormatIn)
{
    switch (FormatIn)
    {
    case TIME_FORMAT_PTS: return PtsValue(ValueIn); break;

    case TIME_FORMAT_US: return uSecValue(ValueIn); break;

    case TIME_FORMAT_27MHz: return VidValue(ValueIn); break;

    default: return uSecValue(INVALID_TIME); break;
    }
}

TimeStamp_c TimeStamp_c::uSecValue(int64_t ValueIn)
{
    StoredValue = ValueIn;
    StoredFormat = TIME_FORMAT_US;
    return *this;
}


TimeStamp_c TimeStamp_c::PtsValue(int64_t ValueIn)
{
    StoredValue = ValueIn;
    StoredFormat = TIME_FORMAT_PTS;
    return *this;
}


TimeStamp_c TimeStamp_c::Pts33Value(int64_t ValueIn)
{
    StoredValue = ValueIn;
    StoredFormat = TIME_FORMAT_PTS;
    *this = this->Pts33();
    return *this;
}



TimeStamp_c TimeStamp_c::VidValue(int64_t ValueIn)
{
    StoredValue = ValueIn;
    StoredFormat = TIME_FORMAT_27MHz;
    return *this;
}

int64_t TimeStamp_c::Value(stm_se_time_format_t FormatOut) const
{
    switch (FormatOut)
    {
    case TIME_FORMAT_PTS  : return PtsValue();

    case TIME_FORMAT_US   : return uSecValue();

    case TIME_FORMAT_27MHz: return VidValue();

    default : return INVALID_TIME;
    }
}


int64_t TimeStamp_c::uSecValue(void) const
{
    //No arithmetic for invalid time stamps, 0
    if ((NotValidTime(StoredValue))
        || (0 == StoredValue))
    {
        return StoredValue;
    }

    switch (StoredFormat)
    {
    case TIME_FORMAT_PTS :
        // T*1000/90 = T*200/18 => (T*200+9)/18
        return (((StoredValue * 200LL) + 9LL) / 18LL);

    // T*1000/90 = T*100/9 => (T*200+9)/18
    // return(((StoredValue * 100LL) + 9LL/2 )/ 9LL);

    case TIME_FORMAT_27MHz :
        // T/27
        return ((StoredValue + (27LL / 2)) / 27LL);

    case TIME_FORMAT_US :
        return (StoredValue);
    }

    return StoredValue; //useless statement to avoid static analysis warning
}

int64_t TimeStamp_c::mSecValue(void) const
{
    //No arithmetic for invalid time stamps, 0
    if ((NotValidTime(StoredValue))
        || (0 == StoredValue))
    {
        return StoredValue;
    }

    switch (StoredFormat)
    {
    case TIME_FORMAT_PTS :
        // T/90 => (T+45)/90
        return ((StoredValue + 45LL) / 90LL);

    case TIME_FORMAT_27MHz:
        // T/27000 => (T + 27000/2) / 27000;
        return ((StoredValue + 13500LL) / 27000LL);

    case TIME_FORMAT_US:
        // T/1000 => (T+500)/1000
        return ((StoredValue + 500LL) / 1000LL);
    }

    return StoredValue; //useless statement to avoid static analysis warning
}

int64_t TimeStamp_c::PtsValue(void) const
{
    //No arithmetic for invalid time stamps, 0
    if ((NotValidTime(StoredValue))
        || (0 == StoredValue))
    {
        return StoredValue;
    }

    switch (StoredFormat)
    {
    case TIME_FORMAT_US   :
        // T*90/1000 => (T*90 + 500)/1000 => (T*9 + 50)/100
        return (((StoredValue * 9LL) + 50LL) / 100LL);

    // T*90/1000 => (T*90 + 500)/1000 => (T*9 + 50)/100
    // return (((StoredValue * 9LL) + 50LL) / 100LL);

    case TIME_FORMAT_27MHz:
        // T/300 => (T+150)/300
        return ((StoredValue + 150LL) / 300LL);

    case TIME_FORMAT_PTS:
        return (StoredValue);
    }

    return StoredValue; //useless statement to avoid static analysis warning
}

int64_t TimeStamp_c::Pts33Value(void) const
{
    //No arithmetic for invalid time stamps, 0
    if ((NotValidTime(StoredValue))
        || (0 == StoredValue))
    {
        return StoredValue;
    }

    return (this->PtsValue() & ((1ULL << 33) - 1));
}


int64_t TimeStamp_c::VidValue(void) const
{
    //No arithmetic for invalid time stamps, 0
    if ((NotValidTime(StoredValue))
        || (0 == StoredValue))
    {
        return StoredValue;
    }

    switch (StoredFormat)
    {
    case TIME_FORMAT_PTS  : return (StoredValue * 300LL);

    case TIME_FORMAT_US   : return (StoredValue * 27LL);

    case TIME_FORMAT_27MHz: return (StoredValue);
    }

    return StoredValue; //useless statement to avoid static analysis warning
}

TimeStamp_c  TimeStamp_c::TimeFormat(stm_se_time_format_t TimeFormatOut) const
{
    return TimeStamp_c(this->Value(TimeFormatOut), TimeFormatOut);
}

TimeStamp_c  TimeStamp_c::uSec(void) const
{
    return TimeStamp_c(this->uSecValue(), TIME_FORMAT_US);
}

TimeStamp_c  TimeStamp_c::Pts(void) const
{
    return TimeStamp_c(this->PtsValue(), TIME_FORMAT_PTS);
}

TimeStamp_c  TimeStamp_c::Pts33(void) const
{
    return TimeStamp_c(this->Pts33Value(), TIME_FORMAT_PTS);
}

TimeStamp_c  TimeStamp_c::Vid(void) const
{
    return TimeStamp_c(this->VidValue(), TIME_FORMAT_27MHz);
}

TimeStamp_c TimeStamp_c::operator+(const TimeStamp_c &TimeStamp) const
{
    TimeStamp_c Sum;

    if ((this->StoredFormat != TimeStamp.StoredFormat)
        || (! this->IsValid())
        || (! TimeStamp.IsValid()))
    {
        //Sum is UNSPECIFIED_TIME
    }
    else
    {
        Sum.StoredValue  = this->StoredValue + TimeStamp.StoredValue;
        Sum.StoredFormat = this->StoredFormat;
    }

    return Sum;
}

TimeStamp_c TimeStamp_c::operator-(const TimeStamp_c &TimeStamp) const
{
    TimeStamp_c Sum;

    if ((this->StoredFormat != TimeStamp.StoredFormat)
        || (! this->IsValid())
        || (! TimeStamp.IsValid()))
    {
        //Sum is UNSPECIFIED_TIME
    }
    else
    {
        Sum.StoredValue  = this->StoredValue - TimeStamp.StoredValue;
        Sum.StoredFormat = this->StoredFormat;
    }

    return Sum;
}


TimeStamp_c TimeStamp_c::operator+(int64_t ToAdd) const
{
    TimeStamp_c Output;

    if (! this->IsValid())
    {
        //UNSPECIFIED_TIME
    }
    else
    {
        Output.StoredValue  = this->StoredValue + ToAdd;
        Output.StoredFormat = this->StoredFormat;
    }

    return Output;
}

TimeStamp_c TimeStamp_c::operator-(int64_t ToAdd) const
{
    TimeStamp_c Output;

    if (!this->IsValid())
    {
        //UNSPECIFIED_TIME
    }
    else
    {
        Output.StoredValue  = this->StoredValue - ToAdd;
        Output.StoredFormat = this->StoredFormat;
    }

    return Output;
}

TimeStampExtrapolator_c::TimeStampExtrapolator_c(stm_se_time_format_t FormatIn)
    : ReferenceTimeStamp()
    , Extrapolation()
    , AccumulatedError(0)
    , LastTimeStamp()
    , Threshold()
{
    ResetExtrapolator(FormatIn);
}

TimeStampExtrapolator_c::TimeStampExtrapolator_c()
    : ReferenceTimeStamp()
    , Extrapolation()
    , AccumulatedError(0)
    , LastTimeStamp()
    , Threshold()
{
    ResetExtrapolator();
}

TimeStampExtrapolator_c::~TimeStampExtrapolator_c()
{
}


void TimeStampExtrapolator_c::SetTimeStampAsReference(TimeStamp_c NewReference)
{
    // save the value as new ref
    ReferenceTimeStamp = NewReference;
    // reset the extrapolation, accumulated error
    Extrapolation = TimeStamp_c(0, Extrapolation.TimeFormat());
    AccumulatedError = 0;
}

void TimeStampExtrapolator_c::ResetExtrapolator(stm_se_time_format_t FormatIn)
{
    Extrapolation = TimeStamp_c(UNSPECIFIED_TIME, FormatIn); // to save FormatIn
    LastTimeStamp = Extrapolation;
    SetTimeStampAsReference(LastTimeStamp);
    ResetJitterThreshold();
}

void TimeStampExtrapolator_c::ResetJitterThreshold(void)
{
    Threshold.Set(1000, TIME_FORMAT_US); //1ms
    // Convert to Internal
    Threshold = Threshold.TimeFormat(Extrapolation.TimeFormat());
}

void TimeStampExtrapolator_c::SetJitterThreshold(TimeStamp_c NewThreshold)
{
    if (!NewThreshold.IsValid())
    {
        SE_ERROR("New threshold is not valid\n");
    }
    else
        // convert tolerance to internal format
    {
        Threshold = NewThreshold.TimeFormat(Extrapolation.TimeFormat());
    }
}


TimeStamp_c TimeStampExtrapolator_c::GetTimeStamp(TimeStamp_c InTimeStamp, uint32_t Nominator, uint32_t Denominator)
{
    bool UseSynthetic = false;
    TimeStamp_c OutTimeStamp = InTimeStamp; //maintained in same format as InputTimeStamp
    // Extrapolated: adds accumulated extrapolation to Reference, in same units as reference
    TimeStamp_c Extrapolated = ReferenceTimeStamp + Extrapolation.TimeFormat(ReferenceTimeStamp.TimeFormat());

    if (!OutTimeStamp.IsValid())
    {
        UseSynthetic = true;
    }
    else
    {
        //Even if Input is valid extra checks:
        if (LastTimeStamp.IsValid() && Extrapolated.IsValid() && OutTimeStamp.IsValid())
        {
            // All computation in InTimeStamp format, converted where/if necessary
            TimeStamp_c RealDelta = OutTimeStamp - LastTimeStamp.TimeFormat(InTimeStamp.TimeFormat());
            TimeStamp_c SyntheticDelta = Extrapolated.TimeFormat(InTimeStamp.TimeFormat()) - LastTimeStamp.TimeFormat(InTimeStamp.TimeFormat());
            TimeStamp_c AbsDeltaDelta = (RealDelta.Value() > SyntheticDelta.Value()) ? (RealDelta - SyntheticDelta) : (SyntheticDelta - RealDelta);

            // Check that the predicted and actual times deviate by no more than the threshold
            if (AbsDeltaDelta.Value() > Threshold.Value(InTimeStamp.TimeFormat()))
            {
                if (0 == OutTimeStamp.Value())
                {
                    //manage Null PTS case: bug13276
                    // In that case, PTS is marked as invalid and predicted timestamp is used instead.
                    UseSynthetic = true;
                    SE_DEBUG(group_timestamps, "Null PTS detected: Using synthetic PTS\n");
                }
                else
                {
                    //standard case: time deviation error, print message and use new time stamp as new reference
                    SE_WARNING("Unexpected change in playback time. Expected %lld ms, got %lld ms (deltas: exp. %lld ms got %lld ms)\n",
                               Extrapolated.mSecValue(), OutTimeStamp.mSecValue(),
                               SyntheticDelta.mSecValue(), RealDelta.mSecValue());
                }
            }
        }
    }

    if (UseSynthetic)
    {
        // Output is replaced with the Extrapolated time stamp, converted if necessary
        OutTimeStamp = Extrapolated.TimeFormat(InTimeStamp.TimeFormat());
        SE_DEBUG(group_timestamps, "Using synthetic PTS for frame : %lld ms (delta %lld ms)\n",
                 OutTimeStamp.mSecValue(),
                 (OutTimeStamp.mSecValue() - LastTimeStamp.mSecValue()));
    }
    else
    {
        //Save the input TimeStamp as a new reference
        SetTimeStampAsReference(InTimeStamp);
    }

    //Update LastTimeStamp
    LastTimeStamp = OutTimeStamp;
    //Extrapolate next timestamp
    UpdateNextExtrapolatedTime(Nominator, Denominator);
    //Already in same format as input
    return OutTimeStamp;
}

/// Reduces N and D by 10^n, maintaining D 2 multiple (for rounding)
#define TS_EXTRAPOLATOR_REDUCE_BY_10N(N,D)           \
{                                    \
    int64_t num, denum;                      \
    while(   ((N)   == (10 *(num =   (N)/10)))       \
      && ((D)   == (10 *(denum = (D)/10)))       \
      && (denum == (2  *(denum / 2))))           \
    {                                \
    (N) = num;                           \
    (D) = denum;                         \
    }                                \
}
void TimeStampExtrapolator_c::UpdateNextExtrapolatedTime(uint32_t Numerator, uint32_t Denominator)
{
    int64_t FrameDuration;

    if (0 == Numerator)
    {
        SE_DEBUG(group_timestamps, "Numerator is 0\n");
        //Nothing to do
        return;
    }

    if (0 == Denominator)
    {
        TimeStamp_c InvalidTimeStamp;
        SE_ERROR("Denominator is 0\n");
        ResetExtrapolator(Extrapolation.TimeFormat());
        return;
    }

    if (LastTimeStamp.IsUnspecifiedTime())
    {
        //If LastTimeStamp is UNSPECIFIED, extrapolated should also be.
        Extrapolation = LastTimeStamp.TimeFormat(Extrapolation.TimeFormat());
        return;
    }

    int64_t TimeMultiplier = 0LL;

    switch (Extrapolation.TimeFormat())
    {
    case TIME_FORMAT_PTS:   TimeMultiplier =    90000ull; break; // 90k * 1Hz

    case TIME_FORMAT_US:    TimeMultiplier =  1000000ull; break; // 1M  * 1Hz

    case TIME_FORMAT_27MHz: TimeMultiplier = 27000000ull; break; // 27M * 1Hz
    }

    TS_EXTRAPOLATOR_REDUCE_BY_10N(TimeMultiplier, Denominator);
    TS_EXTRAPOLATOR_REDUCE_BY_10N(Numerator, Denominator);
    FrameDuration = (((int64_t) Numerator * TimeMultiplier) + (int64_t)(Denominator / 2)) / (int64_t) Denominator;
    AccumulatedError += (Numerator * TimeMultiplier) - (FrameDuration * Denominator);

    if (AccumulatedError > ((int64_t)Denominator * TimeMultiplier))
    {
        FrameDuration++;
        AccumulatedError -= (int64_t) Denominator * TimeMultiplier;
    }

    Extrapolation = Extrapolation + FrameDuration; // in Extrapolation.TimeFormat()
}
#undef TS_EXTRAPOLATOR_REDUCE_BY_10N
