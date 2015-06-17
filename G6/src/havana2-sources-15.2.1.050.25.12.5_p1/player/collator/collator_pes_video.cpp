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
#include "spanning_startcode.h"
#include "collator_pes_video.h"

#define ZERO_START_CODE_HEADER_SIZE     7               // Allow us to see 00 00 01 00 00 01 <other code>

Collator_PesVideo_c::Collator_PesVideo_c(void)
    : Collator_Pes_c()
    , TerminationFlagIsSet(false)
{
    SetGroupTrace(group_collator_video);
}

////////////////////////////////////////////////////////////////////////////
///
///     Handle input, by scanning the start codes, and chunking the data
///
///  \par Note 1:
///
///     Since we are accepting pes encapsulated data, and
///     junking the pes header, we need to accumulate the
///     pes header, before parsing and junking it.
///
///  \par Note 2:
///
///     We also need to accumulate and parse padding headers
///     to allow the skipping of pad data.
///
///  \par Note 3:
///
///     In general we deal with only 4 byte codes, so we do not
///     need to accumulate more than 4 bytes for a code.
///     A special case for this is code Zero. Pes packets may
///     partition the input at any point, so standard start
///     codes can span a pes packet header, this is no problem
///     so long as we check the accumulated bytes alongside new
///     bytes after skipping a pes header.
///
///  \par Note 4:
///
///     The zero code special case, when we see 00 00 01 00, we
///     need to accumulate a further 3 bytes this is because we
///     can have the special case of
///
///             00 00 01 00 00 01 <pes/packing start code>
///
///     where the first start code lead in has a terminal byte
///     later in the stream which may lead to a completely different
///     code. If we see 00 00 01 00 00 01 we always ignore the first
///     code, as in a legal DVB stream this must be followed by a
///     pes/packing code of some sort, we accumulate the 3rd byte to
///     determine which
///
///     \todo This function weighs in at over 450 lines...
///
CollatorStatus_t   Collator_PesVideo_c::DoInput(
    PlayerInputDescriptor_t  *Input,
    unsigned int              DataLength,
    void                     *Data,
    bool                      NonBlocking,
    unsigned int             *DataLengthRemaining)
{
    unsigned int            i;
    CollatorStatus_t        Status;
    unsigned int            Transfer;
    unsigned int            Skip;
    unsigned int            SpanningCount;
    unsigned int            CodeOffset;
    unsigned char           Code;
    bool            Loop;
    bool            BlockTerminate;
    FrameParserHeaderFlag_t HeaderFlags;
    unsigned long long      Now = OS_GetTimeInMicroSeconds();

    HeaderFlags = 0;
    st_relayfs_write_se(ST_RELAY_TYPE_PES_VIDEO_BUFFER, ST_RELAY_SOURCE_SE,
                        (unsigned char *)Data, DataLength, &Now);
    SE_ASSERT(!NonBlocking);
    AssertComponentState(ComponentRunning);

    // If we are running InputJump(), wait until it finish, then resume data injection
    if (OS_MutexIsLocked(&InputJumpLock))
    {
        SE_INFO(group_collator_video, "InputJumpLock locked: Drain process is running! Awaiting unlock..\n");
    }

    OS_LockMutex(&InputJumpLock);
    InputEntry(Input, DataLength, Data, NonBlocking);
    ActOnInputDescriptor(Input);
    //
    // Initialize scan state
    //
    RemainingData       = (unsigned char *)Data;
    RemainingLength     = DataLength;

    while ((RemainingLength != 0) ||
           (GotPartialHeader && (GotPartialCurrentSize >= GotPartialDesiredSize)))
    {
        // Check if not in low power state
        if (Playback->IsLowPowerState())
        {
            // Stop processing data to speed-up low power enter procedure (bug 24248)
            break;
        }

        //
        // Are we accumulating an extended header
        //

        if (GotPartialHeader)
        {
            if (GotPartialCurrentSize < GotPartialDesiredSize)
            {
                Transfer    =  min(RemainingLength, (GotPartialDesiredSize - GotPartialCurrentSize));
                memcpy(StoredPartialHeader + GotPartialCurrentSize, RemainingData, Transfer);
                GotPartialCurrentSize   += Transfer;
                RemainingData       += Transfer;
                RemainingLength     -= Transfer;
            }

            if (GotPartialCurrentSize >= GotPartialDesiredSize)
            {
                Loop    = false;

                switch (GotPartialType)
                {
                case HeaderZeroStartCode:
                    if ((StoredPartialHeader[4] == 0x00) && (StoredPartialHeader[5] == 0x01))
                    {
                        GotPartialType       = (StoredPartialHeader[6] == PES_PADDING_START_CODE) ? HeaderPaddingStartCode : HeaderPesStartCode;
                        GotPartialDesiredSize    = (StoredPartialHeader[6] == PES_PADDING_START_CODE) ? PES_PADDING_INITIAL_HEADER_SIZE : PES_INITIAL_HEADER_SIZE;
                        AccumulatedDataSize     += 3;
                        StoredPartialHeader     += 3;
                        GotPartialCurrentSize    = 4;
                    }
                    else
                    {
                        GotPartialDesiredSize    = 4;

                        if (Configuration.DetermineFrameBoundariesByPresentationToFrameParser)
                        {
                            GotPartialDesiredSize += Stream->GetFrameParser()->RequiredPresentationLength(0x00);
                        }

                        GotPartialType       = HeaderGenericStartCode;
                    }

                    Loop                = true;
                    break;

                case HeaderPesStartCode:
                    if (GotPartialCurrentSize >= PES_INITIAL_HEADER_SIZE)
                    {
                        GotPartialDesiredSize = PES_HEADER_SIZE(StoredPartialHeader);
                    }

                    if (GotPartialCurrentSize < GotPartialDesiredSize)
                    {
                        Loop            = true;
                        break;
                    }

                    GotPartialHeader        = false;
                    StoredPesHeader         = StoredPartialHeader;
                    Status              = ReadPesHeader();

                    if (Status != CollatorNoError)
                    {
                        InputExit();
                        // Unlock to allow InputJump() function to run if need
                        OS_UnLockMutex(&InputJumpLock);
                        return Status;
                    }

                    if (SeekingPesHeader)
                    {
                        AccumulatedDataSize         = 0;            // Dump any collected data
                        SeekingPesHeader            = false;
                    }

                    break;

                case HeaderControlStartCode:
                    StoredPesHeader = StoredPartialHeader;
                    ReadControlUserData();
                    GotPartialHeader    = false;
                    break;

                case HeaderPaddingStartCode:
                    Skipping            = PES_PADDING_SKIP(StoredPartialHeader);
                    GotPartialHeader        = false;
                    break;

                case HeaderGenericStartCode:
                    //
                    // Is it going to terminate a frame
                    //
                    Code                = StoredPartialHeader[3];

                    if (Configuration.DetermineFrameBoundariesByPresentationToFrameParser)
                    {
                        Stream->GetFrameParser()->PresentCollatedHeader(Code, (StoredPartialHeader + 4), &HeaderFlags);
                        BlockTerminate      = (HeaderFlags & FrameParserHeaderFlagPartitionPoint) != 0;
                    }
                    else
                    {
                        BlockTerminate      = (((Code & Configuration.BlockTerminateMask) == Configuration.BlockTerminateCode) && !Configuration.DeferredTerminateFlag) ||
                                              (Configuration.StreamTerminateFlushesFrame && (Code == Configuration.StreamTerminationCode)) ||
                                              (Configuration.DeferredTerminateFlag && TerminationFlagIsSet);
                        TerminationFlagIsSet    = false;
                    }

                    GotPartialHeader        = false;

                    if (BlockTerminate)
                    {
                        memcpy(StoredPartialHeaderCopy, StoredPartialHeader, GotPartialCurrentSize);
                        Status          = InternalFrameFlush();

                        if (Status != CollatorNoError)
                        {
                            InputExit();
                            // Unlock to allow InputJump() function to run if need
                            OS_UnLockMutex(&InputJumpLock);
                            return Status;
                        }

                        memcpy(BufferBase, StoredPartialHeaderCopy, GotPartialCurrentSize);
                        AccumulatedDataSize     = 0;
                        SeekingPesHeader        = false;
                    }

                    //
                    // Accumulate it in any event
                    //
                    Status      = AccumulateStartCode(PackStartCode(AccumulatedDataSize, Code));

                    if (Status != CollatorNoError)
                    {
                        DiscardAccumulatedData();
                        InputExit();
                        // Unlock to allow InputJump() function to run if need
                        OS_UnLockMutex(&InputJumpLock);
                        return Status;
                    }

                    AccumulatedDataSize         += GotPartialCurrentSize;

                    //
                    // Check whether or not this start code will be a block terminate in the future
                    //

                    if (Configuration.DeferredTerminateFlag && ((Code & Configuration.BlockTerminateMask) == Configuration.BlockTerminateCode))
                    {
                        TerminationFlagIsSet = true;
                    }

                    break;
                }

                if (Loop)
                {
                    continue;
                }
            }

            if (RemainingLength == 0)
            {
                InputExit();
                // Unlock to allow InputJump() function to run if need
                OS_UnLockMutex(&InputJumpLock);
                return CollatorNoError;
            }
        }

        //
        // Are we skipping padding
        //

        if (Skipping != 0)
        {
            Skip                 = min(Skipping, RemainingLength);
            RemainingData       += Skip;
            RemainingLength     -= Skip;
            Skipping            -= Skip;

            if (RemainingLength == 0)
            {
                InputExit();
                // Unlock to allow InputJump() function to run if need
                OS_UnLockMutex(&InputJumpLock);
                return CollatorNoError;
            }
        }

        //
        // Check for spanning header
        //
        // Check for a start code spanning, or in the first word
        // record the nature of the span in a counter indicating how many
        // bytes of the code are in the remaining data.
        SpanningCount = ScanForSpanningStartcode(
                            BufferBase, AccumulatedDataSize,
                            RemainingData, RemainingLength);

        if (SpanningCount == 0 && RemainingLength >= 3 && memcmp(RemainingData, "\x00\x00\x01", 3) == 0)
        {
            SpanningCount               = 4;
            UseSpanningTime             = false;
            SpanningPlaybackTimeValid   = false;
            SpanningDecodeTimeValid     = false;
        }

        //
        // Check that if we have a spanning code, that the code is not to be ignored
        //

        if ((SpanningCount != 0) && (SpanningCount <= RemainingLength) &&
            IsCodeTobeIgnored(RemainingData[SpanningCount - 1]))
        {
            SpanningCount       = 0;
        }

        //
        // Handle a spanning start code
        //

        if (SpanningCount != 0)
        {
            //
            // Copy over the spanning bytes
            //
            for (i = 0; i < min(SpanningCount, RemainingLength); i++)
            {
                BufferBase[AccumulatedDataSize + i]     = RemainingData[i];
            }

            if (SpanningCount > RemainingLength)
            {
                RemainingData           += i;
                RemainingLength         -= i;
                AccumulatedDataSize     += i;
                break;
            }

            RemainingData           += i;
            RemainingLength         -= i;
            AccumulatedDataSize     += SpanningCount - 4;
        }
        //
        // Handle search for next start code
        //
        else
        {
            //
            // If we had no spanning code, but we had a spanning PTS, and we
            // had no normal PTS for this frame, then copy the spanning time
            // to the normal time.
            //
            if (!PlaybackTimeValid && SpanningPlaybackTimeValid)
            {
                PlaybackTimeValid       = SpanningPlaybackTimeValid;
                PlaybackTime            = SpanningPlaybackTime;
                DecodeTimeValid         = SpanningDecodeTimeValid;
                DecodeTime          = SpanningDecodeTime;
                UseSpanningTime         = false;
                SpanningPlaybackTimeValid   = false;
                SpanningDecodeTimeValid     = false;
            }

            //
            // Get a new start code
            //
            Status      = FindNextStartCode(&CodeOffset);

            if (Status != CollatorNoError)
            {
                //
                // No start code, copy remaining data into buffer, and exit
                //
                Status  = AccumulateData(RemainingLength, RemainingData);

                if (Status != CollatorNoError)
                {
                    DiscardAccumulatedData();
                }

                RemainingLength         = 0;
                InputExit();
                // Unlock to allow InputJump() function to run if need
                OS_UnLockMutex(&InputJumpLock);
                return Status;
            }

            //
            // Got a start code accumulate up to it, and process
            //
            Status      = AccumulateData(CodeOffset + 4, RemainingData);

            if (Status != CollatorNoError)
            {
                DiscardAccumulatedData();
                InputExit();
                // Unlock to allow InputJump() function to run if need
                OS_UnLockMutex(&InputJumpLock);
                return Status;
            }

            AccumulatedDataSize         -= 4;
            RemainingLength                     -= CodeOffset + 4;
            RemainingData                       += CodeOffset + 4;
        }

        //
        // Now process the code, whether from spanning, or from search
        //
        GotPartialHeader        = true;
        GotPartialCurrentSize       = 4;
        StoredPartialHeader     = BufferBase + AccumulatedDataSize;
        Code                        = StoredPartialHeader[3];

        if (Code == 0x00)
        {
            GotPartialType      = HeaderZeroStartCode;
            GotPartialDesiredSize   = ZERO_START_CODE_HEADER_SIZE;
        }
        else if (IS_PES_START_CODE_VIDEO(Code))
        {
            unsigned int StreamIdentifierCode;
            StreamIdentifierCode = GetStreamIdentifierCode(Code);
            if ((Code & Configuration.StreamIdentifierMask) == StreamIdentifierCode)
            {
                GotPartialType      = HeaderPesStartCode;
                GotPartialDesiredSize   = PES_INITIAL_HEADER_SIZE;
            }
            else
            {
                // Not interested
                GotPartialHeader    = false;
                SeekingPesHeader    = true;
            }
        }
        else if (Code == PES_PADDING_START_CODE)
        {
            GotPartialType      = HeaderPaddingStartCode;
            GotPartialDesiredSize   = PES_PADDING_INITIAL_HEADER_SIZE;
        }
        else  if (Code == PES_START_CODE_CONTROL)
        {
            GotPartialType = HeaderControlStartCode;
            GotPartialDesiredSize = PES_CONTROL_SIZE;
        }
        else if (SeekingPesHeader)
        {
            // If currently seeking a pes header then ignore the last case of a generic header
            GotPartialHeader        = false;
            AccumulatedDataSize     = 0;
        }
        else
        {
            // A generic start code
            GotPartialType      = HeaderGenericStartCode;
            GotPartialDesiredSize    = 4;

            if (Configuration.DetermineFrameBoundariesByPresentationToFrameParser)
            {
                GotPartialDesiredSize += Stream->GetFrameParser()->RequiredPresentationLength(Code);
            }
        }
    }

    InputExit();
    // Unlock to allow InputJump() function to run if need
    OS_UnLockMutex(&InputJumpLock);
    return CollatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The Frame Flush functions
//

CollatorStatus_t   Collator_PesVideo_c::InternalFrameFlush(bool        FlushedByStreamTerminate)
{
    CodedFrameParameters->FollowedByStreamTerminate     = FlushedByStreamTerminate;
    return InternalFrameFlush();
}


// -----------------------

CollatorStatus_t   Collator_PesVideo_c::InternalFrameFlush(void)
{
    AssertComponentState(ComponentRunning);

    CollatorStatus_t Status = Collator_Pes_c::InternalFrameFlush();
    if (Status != CodecNoError)
    {
        return Status;
    }

    SeekingPesHeader                            = true;
    GotPartialHeader                = false;        // New style all but divx
    GotPartialZeroHeader                        = false;        // Old style for divx support only
    GotPartialPesHeader                         = false;
    GotPartialPaddingHeader                     = false;
    Skipping                                    = 0;
    TerminationFlagIsSet = false;

    //
    // at this point we sit (approximately) between frames and should update the PTS/DTS with the values
    // last extracted from the PES header. UseSpanningTime will (or at least should) be true when the
    // frame header spans two PES packets, at this point the frame started in the previous packet and
    // should therefore use the older PTS.
    //
    if (UseSpanningTime)
    {
        if (false == CodedFrameParameters->PlaybackTimeValid)
        {
            CodedFrameParameters->PlaybackTimeValid = SpanningPlaybackTimeValid;
            CodedFrameParameters->PlaybackTime      = SpanningPlaybackTime;
        }
        SpanningPlaybackTimeValid               = false;
        if (false == CodedFrameParameters->DecodeTimeValid)
        {
            CodedFrameParameters->DecodeTimeValid   = SpanningDecodeTimeValid;
            CodedFrameParameters->DecodeTime        = SpanningDecodeTime;
        }
        SpanningDecodeTimeValid                 = false;
    }
    else
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

    return CodecNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      return StreamIdentifier set for the codec
//
unsigned int Collator_PesVideo_c::GetStreamIdentifierCode(unsigned int StreamIdInPesHeader)
{
    //This function will be called for all except for VC1 streams. In vc1, StreamIdInPesHeader
    // is used to check and return the correct identifier for vc1 ap streams based on the stream_id
    // set StreamIdInPesHeader
    return Configuration.StreamIdentifierCode;
}

