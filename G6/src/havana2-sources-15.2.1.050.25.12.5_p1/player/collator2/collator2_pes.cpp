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

#if 0
static unsigned long long      TimeOfStart;
static unsigned long long      BasePts;
static unsigned long long      LastPts;
#endif

#include "st_relayfs_se.h"
#include "collator2_pes.h"

// /////////////////////////////////////////////////////////////////////////
//
//      The Constructor function
//

Collator2_Pes_c::Collator2_Pes_c(void)
    : Collator2_Base_c()
    , GotPartialHeader(false)
    , GotPartialType()
    , CutPartialType()
    , GotPartialCurrentSize(0)
    , GotPartialDesiredSize(0)
    , CutGotPartialDesiredSize(0)
    , CutGotPartialCurrentSize(0)
    , StoredPartialHeader(NULL)
    , StoredPartialHeaderCopy()
    , Skipping(0)
    , RemainingLength(0)
    , RemainingData(0)
    , RemainingSpanningControlData(0)
    , SpanningControlData()
    , PesPacketLength(0)
    , PesPayloadLength(0)
    , PlaybackTimeValid(false)
    , DecodeTimeValid(false)
    , PlaybackTime(0)
    , DecodeTime(0)
    , UseSpanningTime(false)
    , SpanningPlaybackTimeValid(false)
    , SpanningPlaybackTime(0)
    , SpanningDecodeTimeValid(false)
    , SpanningDecodeTime(0)
    , ControlUserData()
    , Bits()
{
#if 0
    SE_INFO(group_collator_video, "New Collator\n");
    TimeOfStart   = INVALID_TIME;
    BasePts       = INVALID_TIME;
    LastPts       = INVALID_TIME;
#endif

    DiscardingData = true;
}

// /////////////////////////////////////////////////////////////////////////
//
//      The Destructor function
//

Collator2_Pes_c::~Collator2_Pes_c(void)
{
    Halt();
}


// /////////////////////////////////////////////////////////////////////////
//
//      The Halt function, give up access to any registered resources
//

CollatorStatus_t   Collator2_Pes_c::Halt(void)
{
    StoredPartialHeader = NULL;
    RemainingData       = NULL;
    return Collator2_Base_c::Halt();
}

// /////////////////////////////////////////////////////////////////////////
//
//      The discard all accumulated data function
//

CollatorStatus_t   Collator2_Pes_c::DiscardAccumulatedData(void)
{
    bool UsePartitionLockMutex = true;

    AssertComponentState(ComponentRunning);

    if (DiscontinuityControlFound)
    {
        /* Test if the PartitionLock mutex is already locked.
           In case of discontinuity control data, it is already locked by
           ProcessInputForward(), so it should not be locked again to avoid falling in
           deadlock case */
        if (OS_MutexIsLocked(&PartitionLock))
        {
            UsePartitionLockMutex = false;
        }
    }

    if (UsePartitionLockMutex)
    {
        OS_LockMutex(&PartitionLock);
    }

    CollatorStatus_t Status = Collator2_Base_c::DiscardAccumulatedData();
    if (Status != CodecNoError)
    {
        if (UsePartitionLockMutex)
        {
            OS_UnLockMutex(&PartitionLock);
        }

        return Status;
    }

    DiscardingData              = true;
    GotPartialHeader            = false;    // New style most video
    Skipping                    = 0;
    UseSpanningTime             = false;
    SpanningPlaybackTimeValid   = false;
    SpanningDecodeTimeValid     = false;

    if (UsePartitionLockMutex)
    {
        OS_UnLockMutex(&PartitionLock);
    }

    return Status;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The discard all accumulated data function
//

CollatorStatus_t   Collator2_Pes_c::InputJump(bool                      SurplusDataInjected,
                                              bool                      ContinuousReverseJump,
                                              bool                      FromDiscontinuityControl)
{
    CollatorStatus_t        Status;
//
    AssertComponentState(ComponentRunning);
//
    Status                      = Collator2_Base_c::InputJump(SurplusDataInjected, ContinuousReverseJump, FromDiscontinuityControl);

    if (Status != CodecNoError)
    {
        return Status;
    }

    PlaybackTimeValid           = false;
    DecodeTimeValid             = false;
    UseSpanningTime             = false;
    SpanningPlaybackTimeValid   = false;
    SpanningDecodeTimeValid     = false;
    return Status;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Protected - Find the next start code (apart from any one at offset 0)
//

CollatorStatus_t   Collator2_Pes_c::FindNextStartCode(
    unsigned int             *CodeOffset,
    unsigned int UpperLimit)
{
    unsigned int    i;
    unsigned int UpperLimitToSearchStartCode;
    unsigned char   IgnoreLower;
    unsigned char   IgnoreUpper;

    //
    // If less than 4 bytes we do not bother
    //

    if (RemainingLength < 4)
    {
        return CollatorError;
    }

    UpperLimitToSearchStartCode = RemainingLength;

    if (UpperLimit != 0xff)
    {
        UpperLimitToSearchStartCode = UpperLimit;
    }

    UseSpanningTime             = false;
    SpanningPlaybackTimeValid   = false;
    SpanningDecodeTimeValid     = false;
    IgnoreLower                 = Configuration.IgnoreCodesRangeStart;
    IgnoreUpper                 = Configuration.IgnoreCodesRangeEnd;

    //
    // Check in body
    //

    for (i = 2; i < (UpperLimitToSearchStartCode - 3); i += 3)
        if (RemainingData[i] <= 1)
        {
            if (RemainingData[i - 1] == 0)
            {
                if ((RemainingData[i - 2] == 0) && (RemainingData[i] == 0x1))
                {
                    if (inrange(RemainingData[i + 1], IgnoreLower, IgnoreUpper))
                    {
                        continue;
                    }

                    *CodeOffset         = i - 2;
                    return CollatorNoError;
                }
                else if ((RemainingData[i + 1] == 0x1) && (RemainingData[i] == 0))
                {
                    if (inrange(RemainingData[i + 2], IgnoreLower, IgnoreUpper))
                    {
                        continue;
                    }

                    *CodeOffset         = i - 1;
                    return CollatorNoError;
                }
            }

            if ((RemainingData[i + 1] == 0) && (RemainingData[i + 2] == 0x1) && (RemainingData[i] == 0))
            {
                if (inrange(RemainingData[i + 3], IgnoreLower, IgnoreUpper))
                {
                    continue;
                }

                *CodeOffset             = i;
                return CollatorNoError;
            }
        }

    //
    // Check trailing conditions
    //

    if (RemainingData[UpperLimitToSearchStartCode - 4] == 0)
    {
        if ((RemainingData[UpperLimitToSearchStartCode - 3] == 0) && (RemainingData[UpperLimitToSearchStartCode - 2] == 1))
        {
            if (!inrange(RemainingData[UpperLimitToSearchStartCode - 1], IgnoreLower, IgnoreUpper))
            {
                *CodeOffset                     = UpperLimitToSearchStartCode - 4;
                return CollatorNoError;
            }
        }
        else if ((UpperLimitToSearchStartCode >= 5) && (RemainingData[UpperLimitToSearchStartCode - 5] == 0) && (RemainingData[UpperLimitToSearchStartCode - 3] == 1))
        {
            if (!inrange(RemainingData[UpperLimitToSearchStartCode - 2], IgnoreLower, IgnoreUpper))
            {
                *CodeOffset                     = UpperLimitToSearchStartCode - 5;
                return CollatorNoError;
            }
        }
    }

    //
    // No matches
    //
    return CollatorError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Protected - Find the next start code (apart from any one at offset 0)
//

CollatorStatus_t   Collator2_Pes_c::FindPreviousStartCode(
    unsigned int             *CodeOffset)
{
    unsigned int    i;
    unsigned char   IgnoreLower;
    unsigned char   IgnoreUpper;

    //
    // If less than 4 bytes we do not bother
    //

    if (RemainingLength < 4)
    {
        return CollatorError;
    }

    IgnoreLower                 = Configuration.IgnoreCodesRangeStart;
    IgnoreUpper                 = Configuration.IgnoreCodesRangeEnd;

    //
    // Check in body
    //

    for (i = (RemainingLength - 4); i >= 2; i -= 3)
        if (RemainingData[i] <= 1)
        {
            if (RemainingData[i - 1] == 0)
            {
                if ((RemainingData[i - 2] == 0) && (RemainingData[i] == 0x1))
                {
                    if (inrange(RemainingData[i + 1], IgnoreLower, IgnoreUpper))
                    {
                        continue;
                    }

                    *CodeOffset         = i - 2;
                    return CollatorNoError;
                }
                else if ((RemainingData[i + 1] == 0x1) && (RemainingData[i] == 0))
                {
                    if (inrange(RemainingData[i + 2], IgnoreLower, IgnoreUpper))
                    {
                        continue;
                    }

                    *CodeOffset         = i - 1;
                    return CollatorNoError;
                }
            }

            if ((RemainingData[i + 1] == 0) && (RemainingData[i + 2] == 0x1) && (RemainingData[i] == 0))
            {
                if (inrange(RemainingData[i + 3], IgnoreLower, IgnoreUpper))
                {
                    continue;
                }

                *CodeOffset             = i;
                return CollatorNoError;
            }
        }

    //
    // Check trailing conditions
    //

    if (RemainingData[1] == 0)
    {
        if ((RemainingLength >= 5) && (RemainingData[2] == 0) && (RemainingData[3] == 1))
        {
            if (!inrange(RemainingData[4], IgnoreLower, IgnoreUpper))
            {
                *CodeOffset                     = 1;
                return CollatorNoError;
            }
        }
        else if ((RemainingData[0] == 0) && (RemainingData[2] == 1))
        {
            if (!inrange(RemainingData[3], IgnoreLower, IgnoreUpper))
            {
                *CodeOffset                     = 0;
                return CollatorNoError;
            }
        }
    }

    //
    // No matches
    //
    return CollatorError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Protected - Find the next start code (apart from any one at offset 0)
//


CollatorStatus_t   Collator2_Pes_c::ReadPesHeader(unsigned char *PesHeader)
{
    unsigned int     Flags;
    PlayerEventRecord_t     AlarmParsedPtsEvent;
    PlayerStatus_t          PlayerStatus;
    //
    // Here we save the current pts state for use only in any
    // picture start code that spans this pes packet header.
    //
    SpanningPlaybackTimeValid   = PlaybackTimeValid;
    SpanningPlaybackTime        = PlaybackTime;
    SpanningDecodeTimeValid     = DecodeTimeValid;
    SpanningDecodeTime          = DecodeTime;
    UseSpanningTime             = true;
    // We have 'consumed' the old values by transferring them to the spanning values.
    PlaybackTimeValid           = false;
    DecodeTimeValid             = false;
    //
    // Read the length of the payload (which for video packets within transport stream may be zero)
    //
    PesPacketLength = (PesHeader[4] << 8) + PesHeader[5];

    if (PesPacketLength)
    {
        PesPayloadLength = PesPacketLength - PesHeader[8] - 3 - Configuration.ExtendedHeaderLength;
    }
    else
    {
        PesPayloadLength = 0;
    }

    //
    // Bits 0xc0 of byte 6 determine PES or system stream, for PES they are always 0x80,
    // for system stream they are never 0x80 (may be a number of other values).
    //

    if ((PesHeader[6] & 0xc0) == 0x80)
    {
        Bits.SetPointer(PesHeader + 9);           // Set bits pointer ready to process optional fields

        //
        // Commence header parsing, moved initialization of bits class here,
        // because code has been added to parse the other header fields, and
        // this assumes that the bits pointer has been initialized.
        //

        if ((PesHeader[7] & 0x80) == 0x80)
        {
            //
            // Read the PTS
            //
            Bits.FlushUnseen(4);
            PlaybackTime         = (unsigned long long)(Bits.Get(3)) << 30;
            Bits.FlushUnseen(1);
            PlaybackTime        |= Bits.Get(15) << 15;
            Bits.FlushUnseen(1);
            PlaybackTime        |= Bits.Get(15);
            Bits.FlushUnseen(1);
            PlaybackTimeValid    = true;
            SE_VERBOSE(group_collator_video, "Stream 0x%p -Read PTS %016llx\n", Stream, PlaybackTime);

            /* In case an alarm is set for next PTS, signal we found it */
            if (AlarmParsedPtsSet)
            {
                AlarmParsedPtsEvent.Code           = EventAlarmParsedPts;
                AlarmParsedPtsEvent.Playback       = Playback;
                AlarmParsedPtsEvent.Stream         = Stream;
                /* Transfer PTS */
                AlarmParsedPtsEvent.PlaybackTime   = PlaybackTime;
                /* Transfer control_req_time0, control_req_time1 and control_req_time2 */
                AlarmParsedPtsEvent.Value[0].UnsignedInt = ControlUserData[0]; //size
                AlarmParsedPtsEvent.Value[1].UnsignedInt = ControlUserData[2];
                AlarmParsedPtsEvent.Value[2].UnsignedInt = ControlUserData[3];
                AlarmParsedPtsEvent.Value[3].UnsignedInt = ControlUserData[4];
                /*SE_INFO(group_collator_video, "PTS %d, REQ_TIME0 %d REQ_TIME1 %d, %d REQ_TIME2 %d, %d\n",
                        PlaybackTime, ControlUserData[2], ControlUserData[3],
                        AlarmParsedPtsEvent.Value[1].UnsignedInt,
                        ControlUserData[2],
                        AlarmParsedPtsEvent.Value[2].UnsignedInt);*/
                PlayerStatus = Stream->SignalEvent(&AlarmParsedPtsEvent);

                if (PlayerStatus != PlayerNoError)
                {
                    SE_ERROR("Stream 0x%p -Failed to signal EventAlarmParsedPts event\n", Stream);
                }

                AlarmParsedPtsSet = false;
            }

            MonitorPCRTiming(PlaybackTime);
#if 0
            {
                unsigned long long Now;
                unsigned int MilliSeconds;
                bool            M0;
                long long       T0;
                bool            M1;
                long long       T1;

                if (NotValidTime(BasePts))
                {
                    BasePts         = PlaybackTime;
                    LastPts         = PlaybackTime;
                }

                Now                 = OS_GetTimeInMicroSeconds();

                if (NotValidTime(TimeOfStart))
                {
                    TimeOfStart     = Now;
                }

                MilliSeconds        = (unsigned int)((Now - TimeOfStart) / 1000);
                M0                  = PlaybackTime < BasePts;
                T0                  = M0 ? (BasePts - PlaybackTime) : (PlaybackTime - BasePts);
                M1                  = PlaybackTime < LastPts;
                T1                  = M0 ? (LastPts - PlaybackTime) : (PlaybackTime - LastPts);
                SE_INFO(group_collator_video, "Stream 0x%p -(%s) PTS - %2d:%02d:%02d.%03d - %016llx - %c%lld - %c%lld\n",
                        Stream,
                        Configuration.CollatorName,
                        (MilliSeconds / 3600000),
                        ((MilliSeconds / 60000) % 60),
                        ((MilliSeconds / 1000) % 60),
                        (MilliSeconds % 1000),
                        PlaybackTime,
                        (M0 ? '-' : ' '),
                        ((T0 * 300) / 27),
                        (M1 ? '-' : ' '),
                        ((T1 * 300) / 27));
                LastPts             = PlaybackTime;
            }
#endif
        }

        if ((PesHeader[7] & 0xC0) == 0xC0)
        {
            //
            // Read the DTS
            //
            Bits.FlushUnseen(4);
            DecodeTime           = (unsigned long long)(Bits.Get(3)) << 30;
            Bits.FlushUnseen(1);
            DecodeTime          |= Bits.Get(15) << 15;
            Bits.FlushUnseen(1);
            DecodeTime          |= Bits.Get(15);
            Bits.FlushUnseen(1);
            DecodeTimeValid      = true;
        }
        else if ((PesHeader[7] & 0xC0) == 0x40)
        {
            SE_ERROR("Stream 0x%p -Malformed pes header contains DTS without PTS\n", Stream);
        }

        // The following code aims at verifying if the Pes packet sub_stream_id is the one required by the collator...
        if (IS_PES_START_CODE_EXTENDED_STREAM_ID(PesHeader[3]))
        {
            // skip the escr data  if any
            if ((PesHeader[7] & 0x20) == 0x20)
            {
                Bits.FlushUnseen(48);
            }

            // skip the es_rate data if any
            if ((PesHeader[7] & 0x10) == 0x10)
            {
                Bits.FlushUnseen(24);
            }

            // skip the dsm trick mode data if any
            if ((PesHeader[7] & 0x8U) == 0x8U)
            {
                Bits.FlushUnseen(8);
            }

            // skip the additional_copy_info data data if any
            if ((PesHeader[7] & 0x4) == 0x4)
            {
                Bits.FlushUnseen(8);
            }

            // skip the pes_crc data data if any
            if ((PesHeader[7] & 0x2) == 0x2)
            {
                Bits.FlushUnseen(16);
            }

            // handle the pes_extension
            if ((PesHeader[7] & 0x1) == 0x1)
            {
                int PesPrivateFlag = Bits.Get(1);
                int PackHeaderFieldFlag = Bits.Get(1);
                int PrgCounterFlag = Bits.Get(1);
                int PstdFlag = Bits.Get(1);
                Bits.FlushUnseen(3);
                int PesExtensionFlag2 = Bits.Get(1);
                Bits.FlushUnseen((PesPrivateFlag ? 128 : 0) + (PackHeaderFieldFlag ? 8 : 0) + (PrgCounterFlag ? 16 : 0) + (PstdFlag ? 16 : 0));

                if (PesExtensionFlag2)
                {
                    Bits.FlushUnseen(8);
                    int StreamIdExtFlag = Bits.Get(1);

                    if (!StreamIdExtFlag)
                    {
                        int SubStreamId = Bits.Get(7);

                        if ((SubStreamId & Configuration.SubStreamIdentifierMask) != Configuration.SubStreamIdentifierCode)
                        {
                            // Get rid of this packet !
                            return (CollatorError);
                        }
                    }
                }
            }
        }
    }
    //
    // Alternatively read a system stream
    //
    else
    {
        Bits.SetPointer(PesHeader + 6);

        while (Bits.Show(8) == 0xff)
        {
            Bits.Flush(8);
        }

        if (Bits.Show(2) == 0x01)
        {
            Bits.Flush(2);
            Bits.FlushUnseen(1);                // STD scale
            Bits.FlushUnseen(13);               // STD buffer size
        }

        Flags   = Bits.Get(4);

        if ((Flags == 0x02) || (Flags == 0x03))
        {
            PlaybackTime         = (unsigned long long)(Bits.Get(3)) << 30;
            Bits.FlushUnseen(1);
            PlaybackTime        |= Bits.Get(15) << 15;
            Bits.FlushUnseen(1);
            PlaybackTime        |= Bits.Get(15);
            Bits.FlushUnseen(1);
            PlaybackTimeValid    = true;
            MonitorPCRTiming(PlaybackTime);
        }

        if (Flags == 0x03)
        {
            Bits.FlushUnseen(4);
            DecodeTime           = (unsigned long long)(Bits.Get(3)) << 30;
            Bits.FlushUnseen(1);
            DecodeTime          |= Bits.Get(15) << 15;
            Bits.FlushUnseen(1);
            DecodeTime          |= Bits.Get(15);
            Bits.FlushUnseen(1);
            DecodeTimeValid      = true;
        }
    }

//    if (PlaybackTimeValid)
//        SE_ERROR( "Playbacktime Vid = %lld\n",PlaybackTime);
    return CollatorNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Protected - Check for the control data types and process them
//

CollatorStatus_t   Collator2_Pes_c::ReadControlUserData(unsigned char  *PesHeader, bool *NeedToRemoveControlFromESBuffer)
{
    unsigned int DataLength;
    *NeedToRemoveControlFromESBuffer = true;

    // Check that the start code we found has the right syntax for a control data
    // Byte 6 is the PES_packet_length
    // bit 1 on byte 8 is the PES_extension_flag
    // Bytes 11 to 14 are "STMM" pattern. Aim is to help control data recognition
    if ((PesHeader[5] == 0x14) && ((PesHeader[7] & 0x01) == 0x01)
        && (PesHeader[10] == 0x53) && (PesHeader[11] == 0x54) &&
        (PesHeader[12] == 0x4D) && (PesHeader[13] == 0x4D))
    {
        // Byte 9 is the PES_header_data_length
        DataLength = PesHeader[8];
        // user data size is given on byte 9 + PES_header_data_length
        ControlUserData[0] = PesHeader[ 8 + DataLength];

        if ((ControlUserData[0] == 0xff) || (ControlUserData[0] == 0x00))
        {
            ControlUserData[0] = 8;
        }

        if (ControlUserData[0] < 11)
        {
            // Byte 15 is control data type, on 1 byte
            ControlUserData[1] = (unsigned int) PesHeader[14];

            if (ControlUserData[1] == PES_CONTROL_REQ_TIME)
            {
                // Control_Req_Time0 is 4 byte long, from byte 16 to 19
                ControlUserData[2] = (((unsigned int)PesHeader[15]) << 24) + (((unsigned int)PesHeader[16]) << 16) + (((unsigned int)PesHeader[17]) << 8) + ((unsigned int)PesHeader[18]);
                // Control_Req_Time1 is 4 byte long, from byte 20 to 23
                ControlUserData[3] = (((unsigned int)PesHeader[19]) << 24) + (((unsigned int)PesHeader[20]) << 16) + (((unsigned int)PesHeader[21]) << 8) + ((unsigned int)PesHeader[22]);
                // We could have an extension of user data on 2 more bytes
                ControlUserData[4] = (((unsigned int)PesHeader[23]) << 24) + (((unsigned int)PesHeader[24]) << 16);
                //SE_INFO(group_collator_video, "ControlUserData[2]= 0x%08x, ControlUserData[3]= 0x%08x\n", ControlUserData[2], ControlUserData[3]);
                SetAlarmParsedPts();
            }
            else if ((ControlUserData[1] == PES_CONTROL_BRK_FWD) || (ControlUserData[1] == PES_CONTROL_BRK_BWD))
            {
                // Set FromDiscontinuityControl parameter to true
                // The only application using control user data can guarantee that injection finishes on a frame boundary.
                // So setting surplus data parameter to false.
                InputJump(false, false, true);
                *NeedToRemoveControlFromESBuffer = false;
#ifndef UNITTESTS
                Stream->DisplayDiscontinuity = true;
#endif

                if (ControlUserData[1] == PES_CONTROL_BRK_FWD)
                    st_relayfs_write_se(ST_RELAY_TYPE_CONTROL_FWD_DISCONTINUITY, ST_RELAY_SOURCE_CONTROL_TEST_DISCONTINUITY,
                                        (unsigned char *)ST_RELAY_CONTROL_DISCONTINUITY_TEXT,
                                        ST_RELAY_CONTROL_DISCONTINUITY_TEXT_LENGTH, 0);

                if (ControlUserData[1] == PES_CONTROL_BRK_BWD)
                    st_relayfs_write_se(ST_RELAY_TYPE_CONTROL_BWD_DISCONTINUITY, ST_RELAY_SOURCE_CONTROL_TEST_DISCONTINUITY,
                                        (unsigned char *)ST_RELAY_CONTROL_DISCONTINUITY_TEXT,
                                        ST_RELAY_CONTROL_DISCONTINUITY_TEXT_LENGTH, 0);
            }
            else if (ControlUserData[1] == PES_CONTROL_BRK_BWD_SMOOTH)
            {
                // Set FromDiscontinuityControl parameter to true
                // The only application using control user data can guarantee that injection finishes on a frame boundary.
                // So setting surplus data parameter to false.
                InputJump(false, true, true);
                *NeedToRemoveControlFromESBuffer = false;
                st_relayfs_write_se(ST_RELAY_TYPE_CONTROL_BWD_SMOOTH_DISCONTINUITY, ST_RELAY_SOURCE_CONTROL_TEST_DISCONTINUITY,
                                    (unsigned char *)ST_RELAY_CONTROL_DISCONTINUITY_TEXT,
                                    ST_RELAY_CONTROL_DISCONTINUITY_TEXT_LENGTH, 0);
            }
            else if (ControlUserData[1] == PES_CONTROL_BRK_SPLICING)
            {
                // A splicing control data is found. Read its associated data
                //  and insert a marker frame to output port.
                CollatorStatus_t Status;
                stm_marker_splicing_data_t SplicingMarkerData;
                SplicingMarkerData.splicing_flags = (unsigned int)PesHeader[15];
                SplicingMarkerData.PTS_offset = ((int64_t)PesHeader[20] << 32) + (((uint64_t)PesHeader[16]) << 24)
                                                + (((uint64_t)PesHeader[17]) << 16) + (((uint64_t)PesHeader[18]) << 8) + ((uint64_t)PesHeader[19]);

                // Sign extend from 40 bits to 64 bits
                SplicingMarkerData.PTS_offset = (SplicingMarkerData.PTS_offset << 24) >> 24;

                //Partition the last data out before the splicing marker
                AccumulateOnePartition();
                PartitionPointSafeToOutputCount = PartitionPointUsedCount;
                PartitionOutput(0);

                SE_VERBOSE(group_collator_video, "Stream 0x%p - Inserting last live data before splicing marker\n", Stream);

                Status = InsertMarkerFrameToOutputPort(Stream, SplicingMarker, (void *) &SplicingMarkerData);
                if (Status != CollatorNoError)
                {
                    SE_VERBOSE(group_collator_video, "Stream 0x%p - Insert of splicing video marker failed \n", Stream);
                    return Status;
                }

                *NeedToRemoveControlFromESBuffer = false;

                /* Do not set Stream->DisplayDiscontinuity to true as it will not be synchronous with the picture it should affect */
                /* Need to clarify if display module needs the discontinuity flag to be set in this case. */

                st_relayfs_write_se(ST_RELAY_TYPE_CONTROL_SPLICING, ST_RELAY_SOURCE_CONTROL_TEST_DISCONTINUITY,
                                    (unsigned char *)ST_RELAY_CONTROL_SPLICING_TEXT,
                                    ST_RELAY_CONTROL_SPLICING_TEXT_LENGTH, 0);
            }

            return CollatorNoError;
        }
        else
        {
            SE_ERROR("This syntax is not supported yet\n");
        }

        // Returning an error is not the right thing to do
        // We need to inform user that this control data type is not supported. Send an event?
        return CollatorBadControlType;
    }
    else
    {
        // This is not a control data. It doesn't fulfill the whole syntax.
        // Remove the header and copy data to ES buffer
        return CollatorNotAControlData;
    }
}

// /////////////////////////////////////////////////////////////////////////
//
//      Protected - Set an alarm on chunk ID detection to send a message
//  with the following PTS
//

CollatorStatus_t    Collator2_Pes_c::SetAlarmParsedPts(void)
{
    AlarmParsedPtsSet = true;
    return  CollatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Protected - Check if the control data is cutting a start code.
//  If it is the case, remove the control data and repositionned RemainingData
//  pointer to process the cut start code
//

CollatorStatus_t Collator2_Pes_c::SearchForStartCodeCutByControlData(unsigned int AdditionalBytes)
{
    PartitionPoint_t    *PreviousPartition;
    unsigned int NumberOfBytesInPreviousPartition;
    unsigned int FirstByte;
    unsigned int SecondByte;
    unsigned int ThirdByte;
    StoredPartialHeader     = NextPartition->PartitionBase + NextPartition->PartitionSize - GotPartialCurrentSize;

    // case 1 :  00 Control_Data 00 01 code
    if (NextPartition->PartitionSize > (PES_CONTROL_SIZE + 4 + AdditionalBytes))
    {
        CutStartCode = RemainingData[-1 - AdditionalBytes];

        if ((RemainingData[-2 - AdditionalBytes] == 1)
            && (RemainingData[-3 - AdditionalBytes] == 0) && (StoredPartialHeader[-1] == 0))
        {
            if (!inrange(CutStartCode, Configuration.IgnoreCodesRangeStart, Configuration.IgnoreCodesRangeEnd))
            {
                // If the cut start code is not in between IgnoreCodesRangeStart and IgnoreCodesRangeEnd
                // there is no need to process it
                RemoveControlAndReposition(AdditionalBytes, StartCodeCutAfterFirstByte);
            }
        }
    }
    // Not all data are present in the current partition. Check in the previous one, if it exists.
    else if (PartitionPointUsedCount > 1)
    {
        NumberOfBytesInPreviousPartition = PES_CONTROL_SIZE + 4 + AdditionalBytes - NextPartition->PartitionSize;
        PreviousPartition = &PartitionPoints[PartitionPointUsedCount - 1];
        StoredPartialHeader = PreviousPartition->PartitionBase + PreviousPartition->PartitionSize - NumberOfBytesInPreviousPartition;

        if (NextPartition->PartitionSize > AdditionalBytes)
        {
            CutStartCode = RemainingData[-1 - AdditionalBytes];
        }
        else
        {
            CutStartCode = StoredPartialHeader[PES_CONTROL_SIZE + 3];
        }

        FirstByte = StoredPartialHeader[0];

        if (NextPartition->PartitionSize > (2 + AdditionalBytes))
        {
            SecondByte = RemainingData[-3 - AdditionalBytes];
        }
        else
        {
            SecondByte = StoredPartialHeader[PES_CONTROL_SIZE + 1];
        }

        if (NextPartition->PartitionSize > (1 + AdditionalBytes))
        {
            ThirdByte = RemainingData[-2 - AdditionalBytes];
        }
        else
        {
            ThirdByte = StoredPartialHeader[PES_CONTROL_SIZE + 2];
        }

        if ((FirstByte == 0) && (SecondByte == 0) && (ThirdByte == 1))
        {
            if (!inrange(CutStartCode, Configuration.IgnoreCodesRangeStart, Configuration.IgnoreCodesRangeEnd))
            {
                // If the cut start code is not in between IgnoreCodesRangeStart and IgnoreCodesRangeEnd
                // there is no need to process it
                RemoveControlAndReposition(AdditionalBytes, StartCodeCutAfterFirstByte);
            }
        }
    }

    // case 2 :  00 00 Control_Data 01 code X
    if (NextPartition->PartitionSize > (PES_CONTROL_SIZE + 5 + AdditionalBytes))
    {
        if ((RemainingData[-3 - AdditionalBytes] == 1)
            && (StoredPartialHeader[-2] == 0) && (StoredPartialHeader[-1] == 0))
        {
            CutStartCode = RemainingData[-2 - AdditionalBytes];

            if (!inrange(CutStartCode, Configuration.IgnoreCodesRangeStart, Configuration.IgnoreCodesRangeEnd))
            {
                // If the cut start code is not in between IgnoreCodesRangeStart and IgnoreCodesRangeEnd
                // there is no need to process it
                RemoveControlAndReposition(AdditionalBytes, StartCodeCutAfterSecondByte);
            }
        }
    }
    // Not all data are present in the current partition. Check in the previous one, if it exists.
    else if (PartitionPointUsedCount > 1)
    {
        NumberOfBytesInPreviousPartition = PES_CONTROL_SIZE + 5 + AdditionalBytes - NextPartition->PartitionSize;
        PreviousPartition = &PartitionPoints[PartitionPointUsedCount - 1];
        StoredPartialHeader = PreviousPartition->PartitionBase + PreviousPartition->PartitionSize - NumberOfBytesInPreviousPartition;

        if (NextPartition->PartitionSize > (AdditionalBytes + 1))
        {
            CutStartCode = RemainingData[-2 - AdditionalBytes];
        }
        else
        {
            CutStartCode = StoredPartialHeader[PES_CONTROL_SIZE + 3];
        }

        FirstByte = StoredPartialHeader[0];

        if (NumberOfBytesInPreviousPartition > 1)
        {
            SecondByte = StoredPartialHeader[1];
        }
        else
        {
            SecondByte = RemainingData[-3 - PES_CONTROL_SIZE - AdditionalBytes];
        }

        if (NextPartition->PartitionSize > AdditionalBytes + 2)
        {
            ThirdByte = RemainingData[-3 - AdditionalBytes];
        }
        else
        {
            ThirdByte = StoredPartialHeader[PES_CONTROL_SIZE + 2];
        }

        if ((FirstByte == 0) && (SecondByte == 0) && (ThirdByte == 1))
        {
            if (!inrange(CutStartCode, Configuration.IgnoreCodesRangeStart, Configuration.IgnoreCodesRangeEnd))
            {
                // If the cut start code is not in between IgnoreCodesRangeStart and IgnoreCodesRangeEnd
                // there is no need to process it
                RemoveControlAndReposition(AdditionalBytes, StartCodeCutAfterSecondByte);
            }
        }
    }

    // case 3 :  00 00 01 Control_Data code X X
    if (NextPartition->PartitionSize > (PES_CONTROL_SIZE + 6 + AdditionalBytes))
    {
        if ((StoredPartialHeader[-3 - AdditionalBytes] == 0)
            && (StoredPartialHeader[-2 - AdditionalBytes] == 0) && (StoredPartialHeader[-1 - AdditionalBytes] == 1))
        {
            CutStartCode = RemainingData[-3 - AdditionalBytes];

            if (!inrange(CutStartCode, Configuration.IgnoreCodesRangeStart, Configuration.IgnoreCodesRangeEnd))
            {
                // If the cut start code is not in between IgnoreCodesRangeStart and IgnoreCodesRangeEnd
                // there is no need to process it
                RemoveControlAndReposition(AdditionalBytes, StartCodeCutAfterThirdByte);
            }
        }
    }
    // Not all data are present in the current partition. Check in the previous one, if it exists.
    else if (PartitionPointUsedCount > 1)
    {
        NumberOfBytesInPreviousPartition = PES_CONTROL_SIZE + 6 + AdditionalBytes - NextPartition->PartitionSize;
        PreviousPartition = &PartitionPoints[PartitionPointUsedCount - 1];
        StoredPartialHeader = PreviousPartition->PartitionBase + PreviousPartition->PartitionSize - NumberOfBytesInPreviousPartition;

        if (NextPartition->PartitionSize > (AdditionalBytes + 2))
        {
            CutStartCode = RemainingData[-3 - AdditionalBytes];
        }
        else
        {
            CutStartCode = StoredPartialHeader[PES_CONTROL_SIZE + 3];
        }

        FirstByte = StoredPartialHeader[0];

        if (NumberOfBytesInPreviousPartition > 1)
        {
            SecondByte = StoredPartialHeader[1];
        }
        else
        {
            SecondByte = RemainingData[-AdditionalBytes - PES_CONTROL_SIZE - 5];
        }

        if (NumberOfBytesInPreviousPartition > 2)
        {
            ThirdByte = StoredPartialHeader[2];
        }
        else
        {
            ThirdByte = RemainingData[-AdditionalBytes - PES_CONTROL_SIZE - 4];
        }

        if ((FirstByte == 0) && (SecondByte == 0) && (ThirdByte == 1))
        {
            if (!inrange(CutStartCode, Configuration.IgnoreCodesRangeStart, Configuration.IgnoreCodesRangeEnd))
            {
                // If the cut start code is not in between IgnoreCodesRangeStart and IgnoreCodesRangeEnd
                // there is no need to process it
                RemoveControlAndReposition(AdditionalBytes, StartCodeCutAfterThirdByte);
            }
        }
    }

    return CollatorNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Protected - Remove the control data from ES buffer and repositionned
//  RemainingData pointer to process the cut start code
//

CollatorStatus_t    Collator2_Pes_c::RemoveControlAndReposition(unsigned int AdditionalBytes, ControlCuttingStartCodeType_t cutcase)
{
    MoveCurrentPartitionBoundary(-GotPartialCurrentSize);            // Wind the partition back to release the header
    RemainingData = RemainingData - 3 - AdditionalBytes;

    switch (cutcase)
    {
    case StartCodeCutAfterFirstByte:
        // case 1 :  00 Control_Data 00 01 code
        AccumulateData(3, RemainingData);
        RemainingData += 3;
        RemainingLength += AdditionalBytes;
        break;

    case StartCodeCutAfterSecondByte:
        // case 2 :  00 00 Control_Data 01 code X
        AccumulateData(2, RemainingData);
        RemainingData += 2;
        RemainingLength += 1 + AdditionalBytes;
        break;

    case StartCodeCutAfterThirdByte:
        // case 3 :  00 00 01 Control_Data code X X
        AccumulateData(1, RemainingData);
        RemainingData += 1;
        RemainingLength += 2 + AdditionalBytes;
        break;

    default:
        SE_ERROR("Error - Cut case not supported\n");
        break;
    }

    StartCodeCutByControl = true;
    return CollatorNoError;
}
