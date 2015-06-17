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

#include "st_relayfs_se.h"
#include "collator_pes.h"

#if 0
static unsigned long long      TimeOfStart[2];
static unsigned long long      LastTime[2];
static unsigned long long      BasePts[2];
static unsigned long long      LastPts[2];
#endif

Collator_Pes_c::Collator_Pes_c(void)
    : Collator_Base_c()
    , SeekingPesHeader(true)
    , GotPartialHeader(false)
    , GotPartialType()
    , GotPartialCurrentSize()
    , GotPartialDesiredSize()
    , StoredPartialHeader(NULL)
    , StoredPartialHeaderCopy()
    , GotPartialZeroHeader(false)
    , GotPartialZeroHeaderBytes()
    , StoredZeroHeader(NULL)
    , GotPartialPesHeader(false)
    , GotPartialPesHeaderBytes()
    , StoredPesHeader(NULL)
    , GotPartialPaddingHeader(false)
    , GotPartialPaddingHeaderBytes()
    , StoredPaddingHeader(NULL)
    , Skipping(0)
    , RemainingLength(0)
    , RemainingData(NULL)
    , PesPacketLength(0)
    , PesPayloadLength(0)
    , PlaybackTimeValid(false)
    , DecodeTimeValid(false)
    , PlaybackTime(INVALID_TIME)
    , DecodeTime(INVALID_TIME)
    , UseSpanningTime(false)
    , SpanningPlaybackTimeValid(false)
    , SpanningPlaybackTime(INVALID_TIME)
    , SpanningDecodeTimeValid(false)
    , SpanningDecodeTime(INVALID_TIME)
    , ControlUserData()
    , Bits()
    , AudioDescriptionInfo()
    , IsMpeg4p2Stream(false)
{
#if 0
    SE_INFO(GetGroupTrace(), "New Collator\n");
    TimeOfStart[0]  = TimeOfStart[1]    = INVALID_TIME;
    LastTime[0] = LastTime[1]       = INVALID_TIME;
    BasePts[0]  = BasePts[1]        = INVALID_TIME;
    LastPts[0]  = LastPts[1]        = INVALID_TIME;
#endif
}

Collator_Pes_c::~Collator_Pes_c(void)
{
    Halt();
}


// /////////////////////////////////////////////////////////////////////////
//
//      The Halt function, give up access to any registered resources
//

CollatorStatus_t   Collator_Pes_c::Halt(void)
{
    StoredPesHeader     = NULL;
    StoredPaddingHeader = NULL;
    RemainingData       = NULL;
    return Collator_Base_c::Halt();
}

// /////////////////////////////////////////////////////////////////////////
//
//      The discard all accumulated data function
//

CollatorStatus_t   Collator_Pes_c::DiscardAccumulatedData(void)
{
    AssertComponentState(ComponentRunning);

    CollatorStatus_t Status = Collator_Base_c::DiscardAccumulatedData();
    if (Status != CodecNoError)
    {
        return Status;
    }

    SeekingPesHeader            = true;
    GotPartialHeader            = false;    // New style most video
    GotPartialZeroHeader        = false;    // Old style used by divx only
    GotPartialPesHeader         = false;
    GotPartialPaddingHeader     = false;
    Skipping                    = 0;
    UseSpanningTime             = false;
    SpanningPlaybackTimeValid   = false;
    SpanningDecodeTimeValid     = false;
    return Status;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The discard all accumulated data function
//

CollatorStatus_t   Collator_Pes_c::InputJump(bool                      SurplusDataInjected,
                                             bool                      ContinuousReverseJump,
                                             bool                      FromDiscontinuityControl)
{
    AssertComponentState(ComponentRunning);

    CollatorStatus_t Status = Collator_Base_c::InputJump(SurplusDataInjected, ContinuousReverseJump, FromDiscontinuityControl);
    if (Status != CodecNoError)
    {
        return Status;
    }

    PlaybackTimeValid           = false;
    DecodeTimeValid             = false;
    UseSpanningTime             = false;
    SpanningPlaybackTimeValid   = false;
    SpanningDecodeTimeValid     = false;
    SeekingPesHeader        = true;
    return Status;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Protected - Find the next start code (apart from any one at offset 0)
//

CollatorStatus_t   Collator_Pes_c::FindNextStartCode(
    unsigned int             *CodeOffset)
{
    unsigned int    i;

    //
    // If less than 4 bytes we do not bother
    //

    if (RemainingLength < 4)
    {
        return CollatorError;
    }

    UseSpanningTime             = false;
    SpanningPlaybackTimeValid   = false;
    SpanningDecodeTimeValid     = false;

    //
    // Check in body
    //

    for (i = 2; i < (RemainingLength - 3); i += 3)
        if (RemainingData[i] <= 1)
        {
            if (RemainingData[i - 1] == 0)
            {
                if ((RemainingData[i - 2] == 0) && (RemainingData[i] == 0x1))
                {
                    if (IsCodeTobeIgnored(RemainingData[i + 1]))
                    {
                        continue;
                    }

                    *CodeOffset         = i - 2;
                    return CollatorNoError;
                }
                else if ((RemainingData[i + 1] == 0x1) && (RemainingData[i] == 0))
                {
                    if (IsCodeTobeIgnored(RemainingData[i + 2]))
                    {
                        continue;
                    }

                    *CodeOffset         = i - 1;
                    return CollatorNoError;
                }
            }

            if ((RemainingData[i + 1] == 0) && (RemainingData[i + 2] == 0x1) && (RemainingData[i] == 0))
            {
                if (IsCodeTobeIgnored(RemainingData[i + 3]))
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

    if (RemainingData[RemainingLength - 4] == 0)
    {
        if ((RemainingData[RemainingLength - 3] == 0) && (RemainingData[RemainingLength - 2] == 1))
        {
            if (!IsCodeTobeIgnored(RemainingData[RemainingLength - 1]))
            {
                *CodeOffset                     = RemainingLength - 4;
                return CollatorNoError;
            }
        }
        else if ((RemainingLength >= 5) && (RemainingData[RemainingLength - 5] == 0) && (RemainingData[RemainingLength - 3] == 1))
        {
            if (!IsCodeTobeIgnored(RemainingData[RemainingLength - 2]))
            {
                *CodeOffset                     = RemainingLength - 5;
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

#define MarkerBits( n, v )  if( Bits.Get((n)) != (v) )                                                                      \
                                {                                                                                               \
                                    SE_ERROR( "Invalid marker bits value %d\n", __LINE__ );   \
                    report_dump_hex( severity_info, StoredPesHeader, 32, 16, NULL ); \
                    PlaybackTimeValid           = SpanningPlaybackTimeValid;                    \
                    PlaybackTime                = SpanningPlaybackTime;                     \
                    DecodeTimeValid             = SpanningDecodeTimeValid;                  \
                    DecodeTime                  = SpanningDecodeTime;                       \
                    SpanningPlaybackTimeValid   = false;                            \
                    SpanningDecodeTimeValid = false;                            \
                                    return CollatorError;                                                               \
                                }

#define MarkerBit( v )      MarkerBits( 1, v )


CollatorStatus_t   Collator_Pes_c::ReadPesHeader(void)
{
    unsigned int        Flags;
    PlayerEventRecord_t     AlarmParsedPtsEvent;
    PlayerStatus_t          PlayerStatus;
    //
    // Just check before we begin, that the stream identifier is the one we are interested in
    //
#if 0

    // We really want to do this, but some audio codecs lie about the values and we los their audio.
    if ((StoredPesHeader[3] & Configuration.StreamIdentifierMask) != Configuration.StreamIdentifierCode)
    {
        SE_ERROR("Wrong stream identifier %02x (%02x %02x)\n", StoredPesHeader[3], Configuration.StreamIdentifierCode, Configuration.StreamIdentifierMask);
        return CollatorError;
    }

#endif
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
    PesPacketLength = (StoredPesHeader[4] << 8) + StoredPesHeader[5];

    if (PesPacketLength)
    {
        PesPayloadLength = PesPacketLength - StoredPesHeader[8] - 3 - Configuration.ExtendedHeaderLength;
    }
    else
    {
        PesPayloadLength = 0;
    }

    SE_DEBUG(GetGroupTrace(), "PesPacketLength %d; PesPayloadLength %d\n", PesPacketLength, PesPayloadLength);

    //
    // Bits 0xc0 of byte 6 determine PES or system stream, for PES they are always 0x80,
    // for system stream they are never 0x80 (may be a number of other values).
    //

    if ((StoredPesHeader[6] & 0xc0) == 0x80)
    {
        Bits.SetPointer(StoredPesHeader + 9);           // Set bits pointer ready to process optional fields

        //
        // Check not scrambled
        //

        if ((StoredPesHeader[6] & 0x20) == 0x20)
        {
            SE_ERROR("Packet scrambled\n");
            PlaybackTimeValid           = SpanningPlaybackTimeValid;                // Pop the spanning values back into the main ones
            PlaybackTime                = SpanningPlaybackTime;
            DecodeTimeValid             = SpanningDecodeTimeValid;
            DecodeTime                  = SpanningDecodeTime;
            SpanningPlaybackTimeValid   = false;
            SpanningDecodeTimeValid = false;
            return CollatorError;
        }

        //
        // Commence header parsing, moved initialization of bits class here,
        // because code has been added to parse the other header fields, and
        // this assumes that the bits pointer has been initialized.
        //

        if ((StoredPesHeader[7] & 0x80) == 0x80)
        {
            //
            // Read the PTS
            //
#if 0
            MarkerBits(4, (StoredPesHeader[7] >> 6));
#else
            // Older version of user software give invalid value for these 4 bits so we don't check them
            Bits.FlushUnseen(4);
#endif
            PlaybackTime         = (unsigned long long)(Bits.Get(3)) << 30;
            MarkerBit(1);
            PlaybackTime        |= Bits.Get(15) << 15;
            MarkerBit(1);
            PlaybackTime        |= Bits.Get(15);
            MarkerBit(1);
            PlaybackTimeValid    = true;

            /* In case an alarm is set for next PTS, signal we found it */
            if (AlarmParsedPtsSet)
            {
                AlarmParsedPtsEvent.Code           = EventAlarmParsedPts;
                AlarmParsedPtsEvent.Playback       = Playback;
                AlarmParsedPtsEvent.Stream         = Stream;
                /* Transfer PTS */
                AlarmParsedPtsEvent.PlaybackTime   = PlaybackTime;
                AlarmParsedPtsEvent.Value[0].UnsignedInt = 0x8;// Value[0] is to indicate the size of the data in byte.
                /* Transfer marker_id0 and marker_id1 */
                AlarmParsedPtsEvent.Value[1].UnsignedInt = ControlUserData[1];
                AlarmParsedPtsEvent.Value[2].UnsignedInt = ControlUserData[2];
                /*SE_ERROR( "PTS %d, M_ID0 %d M_ID1 %d, %d M_ID2 %d, %d\n",
                        PlaybackTime, ControlUserData[0], ControlUserData[1],
                        AlarmParsedPtsEvent.Value[0].UnsignedInt,
                        ControlUserData[2],
                        AlarmParsedPtsEvent.Value[1].UnsignedInt );*/
                PlayerStatus = Stream->SignalEvent(&AlarmParsedPtsEvent);

                if (PlayerStatus != PlayerNoError)
                {
                    SE_ERROR("Failed to signal EventAlarmParsedPts event\n");
                }

                AlarmParsedPtsSet = false;
            }

            MonitorPCRTiming(PlaybackTime);
        }

        if ((StoredPesHeader[7] & 0xC0) == 0xC0)
        {
            //
            // Read the DTS
            //
#if 0
            MarkerBits(4, 1);
#else
            // the TV.Berlin station (at least) transmits this field incorrectly so I have removed the check the 4 bit value
            Bits.FlushUnseen(4);
#endif
            DecodeTime           = (unsigned long long)(Bits.Get(3)) << 30;
            MarkerBit(1);
            DecodeTime          |= Bits.Get(15) << 15;
            MarkerBit(1);
            DecodeTime          |= Bits.Get(15);
            MarkerBit(1);
            DecodeTimeValid      = true;
        }
        else if ((StoredPesHeader[7] & 0xC0) == 0x40)
        {
            SE_ERROR("Malformed pes header contains DTS without PTS\n");
        }

        // The following code aims at verifying if the Pes packet sub_stream_id is the one required by the collator...
        // Also this checks if encoding type is mpeg4p2 then we check the private data flag
        if (((StoredPesHeader[3]) == PES_START_CODE_EXTENDED_STREAM_ID) || ((StoredPesHeader[3] & 0xE0) == 0xC0) || (StoredPesHeader[3] == 0xBD) || (IsMpeg4p2Stream))
        {
            // skip the escr data  if any
            if ((StoredPesHeader[7] & 0x20) == 0x20)
            {
                Bits.FlushUnseen(48);
            }

            // skip the es_rate data if any
            if ((StoredPesHeader[7] & 0x10) == 0x10)
            {
                Bits.FlushUnseen(24);
            }

            // skip the dsm trick mode data if any
            if ((StoredPesHeader[7] & 0x8U) == 0x8U)
            {
                Bits.FlushUnseen(8);
            }

            // skip the additional_copy_info data data if any
            if ((StoredPesHeader[7] & 0x4) == 0x4)
            {
                Bits.FlushUnseen(8);
            }

            // skip the pes_crc data data if any
            if ((StoredPesHeader[7] & 0x2) == 0x2)
            {
                Bits.FlushUnseen(16);
            }

            // handle the pes_extension
            if ((StoredPesHeader[7] & 0x1) == 0x1)
            {
                int PesPrivateFlag = Bits.Get(1);
                int PackHeaderFieldFlag = Bits.Get(1);
                int PrgCounterFlag = Bits.Get(1);
                int PstdFlag = Bits.Get(1);
                Bits.FlushUnseen(3);
                int PesExtensionFlag2 = Bits.Get(1);

                if (PesPrivateFlag)
                {
                    if (IsMpeg4p2Stream)
                    {
                        CodedFrameParameters->IsMpeg4p2MetaDataPresent = Bits.Get(8);
                    }
                    else
                    {
                        unsigned long long ADTextTag;
                        Bits.FlushUnseen(8);
                        //unsigned int ADDesLength = Bits.Get(4);
                        ADTextTag = (unsigned long long)(Bits.Get(32)) << 8 ;
                        ADTextTag |=   Bits.Get(8);
                        AudioDescriptionInfo.ADInfoAvailable = true;

                        if (ADTextTag != 0x4454474144)
                        {
                            Bits.FlushUnseen(48);
                            AudioDescriptionInfo.ADValidFlag = false;
                        }
                        else
                        {
                            unsigned int RevisionTextTag =  Bits.Get(8);
                            AudioDescriptionInfo.ADFadeValue = Bits.Get(8);
                            AudioDescriptionInfo.ADPanValue = Bits.Get(8);
                            AudioDescriptionInfo.ADValidFlag = true;

                            if (RevisionTextTag == 0x32)
                            {
                                AudioDescriptionInfo.ADGainCenter = Bits.Get(8);
                                AudioDescriptionInfo.ADGainFront = Bits.Get(8);
                                AudioDescriptionInfo.ADGainSurround = Bits.Get(8);
                            }
                            else if (RevisionTextTag == 0x31)
                            {
                                Bits.FlushUnseen(24);
                            }
                            else
                            {
                                AudioDescriptionInfo.ADValidFlag = false;
                                Bits.FlushUnseen(24);
                            }
                        }

                        Bits.FlushUnseen(32);
                    }
                }
                else
                {
                    AudioDescriptionInfo.ADInfoAvailable = false;
                }

                if ((AudioDescriptionInfo.ADInfoAvailable == true) && (AudioDescriptionInfo.ADValidFlag == false))
                {
                    AudioDescriptionInfo.ADFadeValue = 0;
                    AudioDescriptionInfo.ADPanValue  = 0;
                    AudioDescriptionInfo.ADGainCenter = 0;
                    AudioDescriptionInfo.ADGainFront = 0;
                    AudioDescriptionInfo.ADGainSurround = 0;
                }

                Bits.FlushUnseen((PackHeaderFieldFlag ? 8 : 0) + (PrgCounterFlag ? 16 : 0) + (PstdFlag ? 16 : 0));

                if (PesExtensionFlag2)
                {
                    Bits.FlushUnseen(8);
                    int StreamIdExtFlag = Bits.Get(1);

                    if (!StreamIdExtFlag)
                    {
                        int SubStreamId = Bits.Get(7); //stream_id_extension

                        if (!inrange((SubStreamId & Configuration.SubStreamIdentifierMask), Configuration.SubStreamIdentifierCodeStart, Configuration.SubStreamIdentifierCodeStop))
                        {
                            SE_INFO(GetGroupTrace(), "Rejected Pes packet of type extended_stream_id with SubStreamId: %x\n", SubStreamId);
                            // Get rid of this packet !
                            PlaybackTimeValid           = SpanningPlaybackTimeValid;              // Pop the spanning values back into the main ones
                            PlaybackTime                = SpanningPlaybackTime;
                            DecodeTimeValid             = SpanningDecodeTimeValid;
                            DecodeTime                  = SpanningDecodeTime;
                            SpanningPlaybackTimeValid = false;
                            SpanningDecodeTimeValid   = false;
                            return CollatorError;
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
        Bits.SetPointer(StoredPesHeader + 6);

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
            MarkerBit(1);
            PlaybackTime        |= Bits.Get(15) << 15;
            MarkerBit(1);
            PlaybackTime        |= Bits.Get(15);
            MarkerBit(1);
            PlaybackTimeValid    = true;
            MonitorPCRTiming(PlaybackTime);
        }

        if (Flags == 0x03)
        {
            MarkerBits(4, 1);
            DecodeTime           = (unsigned long long)(Bits.Get(3)) << 30;
            MarkerBit(1);
            DecodeTime          |= Bits.Get(15) << 15;
            MarkerBit(1);
            DecodeTime          |= Bits.Get(15);
            MarkerBit(1);
            DecodeTimeValid      = true;
        }
    }

//    if (PlaybackTimeValid)
//        SE_ERROR( "Playbacktime Aud = %lld\n", PlaybackTime );

    //
    // We either save it to be loaded at the next buffer, or if we
    // have not started picture aquisition we use it immediately.
    //

    if (AccumulatedDataSize == 0)
    {
        if (false == CodedFrameParameters->PlaybackTimeValid)
        {
            CodedFrameParameters->PlaybackTimeValid = PlaybackTimeValid;
            CodedFrameParameters->PlaybackTime      = PlaybackTime;
        }
        PlaybackTimeValid                       = false;
        if (false == CodedFrameParameters->DecodeTimeValid)
        {
            CodedFrameParameters->DecodeTimeValid   = DecodeTimeValid;
            CodedFrameParameters->DecodeTime        = DecodeTime;
        }
        DecodeTimeValid                         = false;
    }

    return CollatorNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Protected - Check for the marker types and process them
//

CollatorStatus_t   Collator_Pes_c::ReadControlUserData(void)
{
    // Byte 15 is marker type, on 1 byte
    ControlUserData[0] = (unsigned int)StoredPesHeader[14];

    if (ControlUserData[0] == PES_CONTROL_REQ_TIME)
    {
        // markerID0 is 4 byte long, from byte 16 to 19
        ControlUserData[1] = (((unsigned int)StoredPesHeader[15]) << 24) + (((unsigned int)StoredPesHeader[16]) << 16) + (((unsigned int)StoredPesHeader[17]) << 8) + ((unsigned int)StoredPesHeader[18]);
        // markerID1 is 4 byte long, from byte 20 to 23
        ControlUserData[2] = (((unsigned int)StoredPesHeader[19]) << 24) + (((unsigned int)StoredPesHeader[20]) << 16) + (((unsigned int)StoredPesHeader[21]) << 8) + ((unsigned int)StoredPesHeader[22]);
        //SE_ERROR( "ControlUserData[1]= %d, ControlUserData[2]= %d\n", ControlUserData[1], ControlUserData[2] );
        SetAlarmParsedPts();
    }
    else if (ControlUserData[0] == PES_CONTROL_BRK_FWD)
    {
        // The only application using control user data can guarantee that injection finishes on a frame boundary.
        // So setting surplus data parameter to false.
        InputJump(false, false, true);
        st_relayfs_write_se(ST_RELAY_TYPE_CONTROL_FWD_DISCONTINUITY, ST_RELAY_SOURCE_CONTROL_TEST_DISCONTINUITY,
                            (unsigned char *)ST_RELAY_CONTROL_DISCONTINUITY_TEXT,
                            ST_RELAY_CONTROL_DISCONTINUITY_TEXT_LENGTH, 0);
    }

    return CollatorNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Protected - Set an alarm on chunk ID detection to send a message
//  with the following PTS
//

CollatorStatus_t    Collator_Pes_c::SetAlarmParsedPts(void)
{
    AlarmParsedPtsSet = true;
    return  CollatorNoError;
}
