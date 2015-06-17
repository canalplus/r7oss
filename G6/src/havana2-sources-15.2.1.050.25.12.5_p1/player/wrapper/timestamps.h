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
#ifndef TIMESTAMPS_H
#define TIMESTAMPS_H

#include "player.h"
#include "player_types.h"
#include "report.h"
#include "osinline.h"

#undef TRACE_TAG
#define TRACE_TAG   "TimeStamp_c"

/**
 * @brief A class providing interface to manipulate timestamps in
 * different units of time.
 *
 *
 * The timestamp storage unit is decided upon setting the value: the class
 * makes no decision about storage format: the responsability of
 * maintainging resolution and range is with caller.
 *
 * This decision also explains why there is no overload of "operator=" :
 * the result of "=" is always exactly the right value, without dependency
 * on the left value.
 *
 * This also mean that arithmetic operations between TimeStamp_c objects
 * of different storage format return an InvalidTime stamp, as the class
 * does not decide.
 *
 * The internal storage is a unsigned 64 bit integrer:
 *
 * at the highest precision this provides a range of 4+ days in 1/27Mhz
 * units.
 */
class TimeStamp_c
{
public:
    /**
     * An initialising constructor.
     *
     *
     * @param TimestampIn the value, in TimeFormatIn
     * @param TimeFormatIn the format of TimestampIn, will be used for storage
     */
    TimeStamp_c(int64_t TimestampIn, stm_se_time_format_t TimeFormatIn);

    /**
     * Default Contructor: Will set the StoredValue to UNSPECIFIED_TIME
     *
     */
    TimeStamp_c();

    ~TimeStamp_c();


    // Get Functions (get functions have no Value argument)

    /**
     * whether a TimeStamp's value is valid
     *
     * @return true if the TimeStamp's value is valid
     */
    bool IsValid(void) const
    {
        return (ValidTime((uint64_t)StoredValue));
    };

    /**
     * whether a TimeStamp's value is unspecified
     *
     * @return true if the TimeStamp's value is unspecified
     */
    bool IsUnspecifiedTime(void) const
    {
        return (UNSPECIFIED_TIME == (uint64_t)StoredValue);
    };

    /**
     * returns the value in a format, performing conversion if required
     *
     *
     * @param OutTimeFormat the format we want to get the value in
     *
     * @return TimeStamp_c
     */
    int64_t Value(stm_se_time_format_t OutTimeFormat) const ;

    /**
     * returns the value in its native stored format
     *
     *
     * @return uint64_t
     */
    int64_t Value() const
    {
        return StoredValue;
    };

    /**
     * Gets the native Stored Format
     *
     *
     * @return stm_se_time_format_t
     */
    stm_se_time_format_t TimeFormat(void) const
    {return StoredFormat;};

    /**
     * Get StoredValue in micro seconds
     *
     *
     * @return uint64_t
     */
    int64_t uSecValue(void)  const ;

    /**
    * Get value in milli seconds
    *
    *
    * @return uint64_t
    */
    int64_t mSecValue(void)  const ;

    /**
    * Get value in 1/90kHz
    *
    *
    * @return uint64_t
    */
    int64_t PtsValue(void)  const ;

    /**
    * Get value in 1/90kHz, on 33bit
    *
    *
    * @return uint64_t
    */
    int64_t Pts33Value(void) const;


    /**
    * Get value in 1/27MHz
      *
     *
     * @return uint64_t
     */
    int64_t VidValue(void)  const ;


    // Set Functions (set the value of the object)

    // Set funtions return an object so that they can be chained for conversion
    // e.g.
    // converting from uSec to Pts = TimeStamp.uSec(Offset).Pts();

    /**
     * Friendly function so that caller does not have to use switch()
     *
     *
     * @param ValueIn the value of the time stamp, in TimeFormatIn
     * @param TimeFormatIn the format TimestampIn is in
     *
     * @return TimeStamp_c a copy of the object
     */
    TimeStamp_c Set(int64_t ValueIn, stm_se_time_format_t TimeFormatIn);

    /**
     * Sets the object with a value in microsecond, sets the internal storage to
     * microsecond.
     *
     *
     * @param ValueIn the value in microseconds
     *
     * @return TimeStamp_c a copy of the object
     */
    TimeStamp_c uSecValue(int64_t ValueIn);

    /**
     * Sets the object with a value in 1/90kHz, sets the internal storage to 1/90kHz
     *
     *
     * @param ValueIn the value in 1/90kHz
     *
     * @return TimeStamp_c a copy of the object
     */
    TimeStamp_c PtsValue(int64_t ValueIn);

    /**
     * Sets the object with a value in 1/90kHz, then limit it to 33bit and sets the
     * internal storage to 1/90kHz
     *
     *
     * @param ValueIn the value in 1/90kHz
     *
     * @return TimeStamp_c a copy of the object
     */
    TimeStamp_c Pts33Value(int64_t ValueIn);

    /**
     * Sets the object with a value in 1/27MHz, sets the internal storage to 1/27MHz
     *
     *
     * @param ValueIn in 1/27MHz
     *
     * @return TimeStamp_c a copy of the object
     */
    TimeStamp_c VidValue(int64_t ValueIn);


    // Conversions : creates a new TimeStamp object, in the specified format
    // e.g. To convert a Pts time stamp to uSec
    // Timestamp = Timestamp.uSec;
    /**
     * returns a TimeStamp_c converted to requested format
     *
     *
     * e.g Offset = Offset.TimeFormat( InputTimeStamp.TimeFormat() );
     *
     * @param TimeFormatOut the format we want the new object in
     *
     * @return TimeStamp_c a copy of the object, converted to TimeFormatOut
     */
    TimeStamp_c TimeFormat(stm_se_time_format_t TimeFormatOut) const;

    /**
     * returns a TimeStamp_c converted to microsecond
     *
     *
     * @return TimeStamp_c a copy of the object, converted to microsecond
     */
    TimeStamp_c uSec() const;

    /**
     * returns a TimeStamp_c converted to 1/90kHz
     *
     *
     * @return TimeStamp_c a copy of the object, converted to 1/90kHz
     */
    TimeStamp_c Pts() const ;

    /**
     * returns a TimeStamp_c converted to 1/90kHz, on 33bit
     *
     *
     * @return TimeStamp_c a copy of the object, converted to 1/90kHz
     */
    TimeStamp_c Pts33() const ;

    /**
     * returns a TimeStamp_c converted to 1/27MHz
     *
     *
     * @return TimeStamp_c a copy of the object, converted to 1/27MHz
     */
    TimeStamp_c Vid() const;


    // Arithmetics

    /**
     * if( This->StoredFormat == TimeStamp->StoredFormat)
     *   returns a TimeStamp_c with This->StoredValue + TimeStamp->StoredValue at
     *   This->StoredFormat;
     * else
     *  return a TimeStamp_c with UNSPECIFIED_TIME StoredValue
     *
     *
     * @param TimeStamp
     *
     * @return TimeStamp_c the result of the operation
     */
    TimeStamp_c operator+(const TimeStamp_c &TimeStamp) const;

    /**
     * if( This->StoredFormat == TimeStamp->StoredFormat)
     *   returns a TimeStamp_c with This->StoredValue - TimeStamp->StoredValue at
     *   This->StoredFormat;
     * else
     *  return a TimeStamp_c with UNSPECIFIED_TIME StoredValue
     *
     *
     * @param TimeStamp
     *
     * @return TimeStamp_c the result of the operation
     */
    TimeStamp_c operator-(const TimeStamp_c &TimeStamp) const;


    /**
     * Adds a value (has to be in the same format as the TimeStamp_c)
     *
     *
     * @param uint64_t Value to add, in the same format as the TimeStamp
     *
     * @return TimeStamp_c the result of the operation
     */
    TimeStamp_c operator+(const int64_t) const;

    /**
     * Substracts a value (has to be in the same format as the TimeStamp_c)
     *
     *
     * @param uint64_t Value to substract, in the same format as the TimeStamp
     *
     * @return TimeStamp_c the result of the operation
     */
    TimeStamp_c operator-(const int64_t) const;

private:
    int64_t             StoredValue;
    stm_se_time_format_t StoredFormat;
};

/**
 * A class to handle timestamp linear 1 order extrapolation. It should be
 * generic enough to handle audio and video.
 *
 * It has a Jitter Threshold property: above this threshold it considers the
 * time line to be discontinuous
 *
 * Known Limitations:
 *
 * 1/ Internal storage of the extrapolated times is through a single uint64_t:
 * user needs to take care of deciding the time format for the extrapolator to
 * strike the right balance between range ans precision (default time format is
 * microseconds).
 *
 * 2/ Because it manipulates time stamps: so far implementation only
 * extrapolates time forward, real-time: there is no support yet of trick-modes,
 * or backward time.
 */
class TimeStampExtrapolator_c
{
public:
    /**
     * Contructor with a format parameter
     *
     *
     * @param FormatIn the format for the internal storage
     */
    explicit TimeStampExtrapolator_c(stm_se_time_format_t FormatIn);

    /**
     * Default constructor
     *
     */
    TimeStampExtrapolator_c();
    ~TimeStampExtrapolator_c();

    /**
     * Resets the extrapolation jitter threshold to default value
     */
    void ResetJitterThreshold(void);

    /**
     * Resets extrapolation mechanism to initial state.
     */
    void ResetExtrapolator(stm_se_time_format_t FormatIn = TIME_FORMAT_US);

    /**
     * Sets the jitter threshold
     *
     *
     * @param NewThreshold a TimeStamps object representing the tolerance
     */
    void SetJitterThreshold(TimeStamp_c NewThreshold);

    /**
     * If the input timestamp is not valid compared to extrapolator state, it
     * returns an extrapolated timestamp based on extrapolator state, else
     * returns the InputTimeStamp.
     *
     * Also updates the internal state for the next time stamp extrapolation,
     * with the duration of the current frame expressed in seconds by (Nominator
     * / Denominator).
     *
     * @param InputTimeStamp timestamp we want to check against extrapolation
     *               context
     * @param Nominator  nominator of the duration of the current frame to
     *           extrapolate the timestamp of the next frame (e.g. Number
     *           of samples)
     * @param Denominator denominator of the duration of the current frame to
     *            extrapolate the timestamp of the next frame (e.g.
     *            Sampling Frequency)
     *
     * @return TimeStamp_c the best timestamp, in the same format at InputTimeStamp
     */
    TimeStamp_c GetTimeStamp(TimeStamp_c InputTimeStamp, uint32_t Nominator, uint32_t Denominator);


    /**
     * Gets the extrapolation time format
     *
     *
     * @return stm_se_time_format_t
     */
    stm_se_time_format_t TimeFormat(void) const
    {return Extrapolation.TimeFormat();}

    /**
     * Gets the whether there is a valid extrapolated value
     *
     *
     * @return boolean
     */
    bool IsValid(void) const
    {return (Extrapolation.IsValid() && ReferenceTimeStamp.IsValid());}

private:
    /// Last Injected Valid TimeStamp, in same units as injected
    TimeStamp_c ReferenceTimeStamp;
    /// Accumulated Extrapolation, in format specicied at construction
    TimeStamp_c Extrapolation;
    /// Error that is accumulated by calculating the Extrapolation in Extrapolation.TimeFormat()
    /// Used to have better precision on accumulating Extrapolation
    int64_t AccumulatedError;
    /// Previous Time
    /// This is primarily used for debugging timestamp sequencing problems (e.g. calculating deltas between
    /// 'then' and 'now'.
    TimeStamp_c LastTimeStamp;
    /// The maximum value by which the actual Normalised Time Satmp is permitted to differ from the synthesised timestamp
    /// (use to identify bad streams or poor predictions).
    TimeStamp_c Threshold;

    /// Sets the new reference, resets extrapolation to 0, resets errortracker
    void SetTimeStampAsReference(TimeStamp_c NewReference);

    /**
     * Updates the expected extrapolated timestamp of the next frame with a
     * time equal to: (Nominator/Denomitor seconds)
     *
     * This will be used to synthesise a timestamp if this is missing from the
     * subsequent frame.
     *
     * @param Nominator  (e.g. Number of samples)
     * @param Denominator (e.g. Sampling Frequency)
     */
    void UpdateNextExtrapolatedTime(uint32_t Nominator, uint32_t Denominator);
};

#endif /* TIMESTAMPS_H */
