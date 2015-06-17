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

////////////////////////////////////////////////////////////////////////////
/// \class Collator2_PesVideoHevc_c
///
/// Implements initialisation of collator video class for Hevc
///
/// Today this code copies a lot of Collator2_PesVideo_c class modified to
/// fix up issues with StartCode cut by PES startcode from bug 27981
/// This "copy paste" implementation avoid to introduce regressions for now

#include "hevc.h"
#include "collator2_pes_video_hevc.h"

#define ZERO_START_CODE_HEADER_SIZE               9   //  7 in pes_video (without the xx bytes) expanded to 9 Allow us to see 00 00 01 xx xx 00 00 01 <other code>
#define START_CODE_SIZE                           4
#define EXTRA_BYTES_TO_DETECT_SPANNING_START_CODE 5   //  3  in pes_video expanded to 5

//
Collator2_PesVideoHevc_c::Collator2_PesVideoHevc_c(void)
    : Collator2_Pes_c()
    , FoundPESHeader(false)
    , ExtendDesiredSizeForGenericStartCode(false)
    , CopyOfStoredPartialHeader()
{
    Configuration.CollatorName               = "HEVC Collator";
    Configuration.GenerateStartCodeList      = true;
    Configuration.MaxStartCodes              = 310;                 // If someone inserts 16 VPS, 32 SPS and 256 PPS
    Configuration.StreamIdentifierMask       = PES_START_CODE_MASK;
    Configuration.StreamIdentifierCode       = PES_START_CODE_VIDEO;
    Configuration.IgnoreCodesRangeStart      = 0xff;                    // Ignore nothing
    Configuration.IgnoreCodesRangeEnd        = 0x00;
    /* We won't insert any terminate SC */
    /* In H264, a filler data code is inserted, to guarantee that no terminal */
    /* code picture parameter sets will always be followed by a zero byte */
    /* (makes the MoreRsbpData implementation a lot simpler).*/
    /* Not sure this trick is needed for HEVC */
    Configuration.InsertFrameTerminateCode   = false;
    Configuration.TerminalCode               = 38 << 1;
    Configuration.ExtendedHeaderLength       = 0;
}

////////////////////////////////////////////////////////////////////////////
//
//     Handle input, by scanning the start codes, and chunking the data
//
//  Note 1:
//
//     Since we are accepting pes encapsulated data, and
//     junking the pes header, we need to accumulate the
//    pes header, before parsing and junking it.
//
//  Note 2:
//
//    We also need to accumulate and parse padding headers
//    to allow the skipping of pad data.
//
//  Note 3:
//
//    In general we deal with only 4 byte codes, so we do not
//    need to accumulate more than 4 bytes for a code.
//    A special case for this is code Zero. Pes packets may
//    partition the input at any point, so standard start
//    codes can span a pes packet header, this is no problem
//    so long as we check the accumulated bytes alongside new
//    bytes after skipping a pes header.
//
//  Note 4:
//
//    The zero code special case, when we see 00 00 01 00, we
//    need to accumulate a further 3 bytes this is because we
//    can have the special case of
//
//            00 00 01 00 00 01 <pes/packing start code>
//
//    where the first start code lead in has a terminal byte
//    later in the stream which may lead to a completely different
//    code. If we see 00 00 01 00 00 01 we always ignore the first
//    code, as in a legal DVB stream this must be followed by a
//    pes/packing code of some sort, we accumulate the 3rd byte to
//    determine which
//

CollatorStatus_t   Collator2_PesVideoHevc_c::ProcessInputForward(
    unsigned int      DataLength,
    void             *Data,
    unsigned int     *DataLengthRemaining)
{
    CollatorStatus_t        Status;
    unsigned int            MinimumHeadroom;
    unsigned int            Transfer;
    unsigned int            Skip;
    unsigned int            SpanningCount;
    unsigned char           Code;
    unsigned int            CodeOffset;
    unsigned int            InjectedMoreThanRequestedByteNumber;
    FrameParserHeaderFlag_t HeaderFlags;
    bool                    Loop;
    bool                    Exit;
    bool                    BlockTerminate;
    bool                    NeedToRemoveControlFromESBuffer;
    bool                    FoundControlAtInjectionEnd;
    unsigned long long      Now = OS_GetTimeInMicroSeconds();

    st_relayfs_write_se(ST_RELAY_TYPE_PES_VIDEO_BUFFER, ST_RELAY_SOURCE_SE,
                        (unsigned char *)Data, DataLength, &Now);

    //
    // Initialize scan state
    //
    FoundControlAtInjectionEnd = false;
    RemainingData       = (unsigned char *)Data;
    RemainingLength     = DataLength;
    Status      = CollatorNoError;
    while ((RemainingLength != 0) ||
           (GotPartialHeader && (GotPartialCurrentSize >= GotPartialDesiredSize)))
    {
        //
        // Allow any higher priority player processes in
        // NOTE On entry we have the partition lock
        //

        OS_UnLockMutex(&PartitionLock);
        OS_LockMutex(&PartitionLock);

        // Check if not in low power state
        if (Playback->IsLowPowerState())
        {
            // Stop processing data to speed-up low power enter procedure (bug 24248)
            break;
        }

        //
        // Do we have room to accumulate, if not we try passing on the
        // accumulated partitions and extending the buffer we have.
        //

        MinimumHeadroom = GotPartialHeader ? max(MINIMUM_ACCUMULATION_HEADROOM, GotPartialDesiredSize) : MINIMUM_ACCUMULATION_HEADROOM;
        if ((CodedFrameBufferFreeSpace < MinimumHeadroom) || (PartitionPointUsedCount >= (MAXIMUM_PARTITION_POINTS - 1)))
        {
            if (MinimumHeadroom > max(MINIMUM_ACCUMULATION_HEADROOM, (MaximumCodedFrameSize - 16)))
            {
                SE_FATAL("Stream 0x%p -Required headroom too large (%d bytes) - Probable implementation error\n", Stream, MinimumHeadroom);
            }

            Status  = PartitionOutput(0);
            if (Status != CollatorNoError)
            {
                SE_ERROR("Stream 0x%p -Output of partitions failed\n", Stream);
                break;
            }

            if ((CodedFrameBufferFreeSpace < MinimumHeadroom) || (PartitionPointUsedCount >= (MAXIMUM_PARTITION_POINTS - 1)))
            {
                if (!NonBlockingInput)
                {
                    SE_FATAL("Stream 0x%p -About to return CollatorWouldBlock when it is OK to block - Probable implementation error\n", Stream);
                }

                Status  = CollatorWouldBlock;
                break;
            }
        }

        //
        // Are we accumulating an extended header
        //

        if (GotPartialHeader)
        {

            if (GotPartialCurrentSize < GotPartialDesiredSize)
            {
                Transfer    =  min(RemainingLength, (GotPartialDesiredSize - GotPartialCurrentSize));
                Status  = AccumulateData(Transfer, RemainingData);
                if (Status != CollatorNoError)
                {
                    EmptyCurrentPartition();               // Dump any collected data in the current partition
                    InitializePartition();
                    GotPartialHeader        = false;
                    DiscardingData      = false;
                    SE_FATAL("Stream 0x%p -Error in accumulating data\n", Stream);
                    break;
                }

                GotPartialCurrentSize   += Transfer;
                RemainingData       += Transfer;
                RemainingLength     -= Transfer;
            }
            // If control data detection is enabled, and we are at the end of the PES buffer and we found a control data SC,
            // check if we have enough data
            if ((Player->PolicyValue(Playback, Stream, PolicyControlDataDetection) == PolicyValueApply) &&
                (RemainingLength == 0) && (GotPartialType == HeaderControlStartCode) &&
                (((GotPartialCurrentSize >= PES_CONTROL_SIZE) && (!FoundControlInHeader)) ||
                 ((GotPartialCurrentSize >= GotPartialDesiredSize) && (FoundControlInHeader))))
            {
                // There is not enough data to check for a possible start code cut by this control data,
                // so we won't check for a cut start code, just process the control data.
                FoundControlAtInjectionEnd = true;
            }

            // If we have enough data, we can process the detected SC
            // In case control data detection is on, and we are at the end of the injected data, just process the control data
            if ((GotPartialCurrentSize >= GotPartialDesiredSize) || (FoundControlAtInjectionEnd))
            {
                Loop    = false;
                Exit    = false;

                // In case of control data detection policy is enabled, we will search here for a potential control data SC in the start code header.
                // In general case, we want to check if a PES header is cutting a SC.
                if ((Player->PolicyValue(Playback, Stream, PolicyControlDataDetection) == PolicyValueApply)
                    || ((GotPartialType != HeaderPesStartCode) && ExtendDesiredSizeForGenericStartCode))
                {
                    if ((!FoundControlAtInjectionEnd) || ((GotPartialType != HeaderPesStartCode) && ExtendDesiredSizeForGenericStartCode))
                    {
                        // The RemainingLength is too small to check if this start code is cut by a control data
                        // Accumulate data and wait for next injection to check
                        if ((RemainingLength > 0) && (RemainingLength < START_CODE_SIZE))
                        {
                            Status  = AccumulateData(RemainingLength , RemainingData);
                            GotPartialCurrentSize += RemainingLength;
                            RemainingData += RemainingLength;
                            RemainingLength = 0;
                            break;
                        }
                        // Make sure that by reverting RemainingData+START_CODE_SIZE by GotPartialCurrentSize, we don't fall outside given data
                        if ((RemainingLength + GotPartialCurrentSize - START_CODE_SIZE) <= DataLength)
                        {
                            RemainingData = RemainingData + START_CODE_SIZE - GotPartialCurrentSize;
                            // Search for a potential control data SC in the start code header
                            // or a PES header cutting detected SC
                            if (!FoundControlAtInjectionEnd)
                            {
                                Status = FindNextStartCode(&CodeOffset, (GotPartialDesiredSize));
                            }
                            else
                            {
                                Status = FindNextStartCode(&CodeOffset, GotPartialDesiredSize - START_CODE_SIZE);
                            }

                            // Handle case of SC cut by a control data SC
                            if ((Status == CollatorNoError) && (RemainingData[CodeOffset + 3] == PES_START_CODE_CONTROL)
                                && (GotPartialType != HeaderControlStartCode))
                            {
                                FoundControlInHeader = true;
                                Code = PES_START_CODE_CONTROL;
                                CutPartialType = GotPartialType;
                                CutGotPartialDesiredSize = GotPartialDesiredSize;
                                CutGotPartialCurrentSize = GotPartialCurrentSize;
                                StoreCodeOffset = CodeOffset;
                                NeedToStepToSCTreatment = true;
                            }
                            // Handle case of PES header cutting the detected SC
                            if ((Status == CollatorNoError) && IS_PES_START_CODE_VIDEO(RemainingData[CodeOffset + 3])
                                && (CodeOffset < (GotPartialDesiredSize - EXTRA_BYTES_TO_DETECT_SPANNING_START_CODE - START_CODE_SIZE))
                                && ExtendDesiredSizeForGenericStartCode && (GotPartialType != HeaderPesStartCode))
                            {
                                FoundPESHeader = true;
                                CutPartialType = GotPartialType;
                                CutGotPartialDesiredSize = GotPartialDesiredSize;
                                CutGotPartialCurrentSize = GotPartialCurrentSize;
                                StoreCodeOffset = CodeOffset;
                                NeedToStepToSCTreatment = true;
                            }
                            // No control data or PES header found, continue normal process
                            RemainingData = RemainingData - START_CODE_SIZE + GotPartialCurrentSize;
                            // Remove the extra 3 bytes to check for PES header in case of HeaderGenericStartCode
                            // This needs to be done AFTER setting RemainingData to the point it was before the SC search.
                            if ((GotPartialType == HeaderGenericStartCode) && (!FoundPESHeader) && ExtendDesiredSizeForGenericStartCode)
                            {
                                GotPartialDesiredSize -= EXTRA_BYTES_TO_DETECT_SPANNING_START_CODE;
                                GotPartialCurrentSize -= EXTRA_BYTES_TO_DETECT_SPANNING_START_CODE;
                                MoveCurrentPartitionBoundary(-EXTRA_BYTES_TO_DETECT_SPANNING_START_CODE);
                                RemainingData -= EXTRA_BYTES_TO_DETECT_SPANNING_START_CODE;
                                RemainingLength += EXTRA_BYTES_TO_DETECT_SPANNING_START_CODE;
                                ExtendDesiredSizeForGenericStartCode = false;
                            }
                        }
                    }
                }
                StoredPartialHeader     = NextPartition->PartitionBase + NextPartition->PartitionSize - GotPartialCurrentSize;

                if (!NeedToStepToSCTreatment)
                {
                    SE_VERBOSE(group_collator_video, "Stream 0x%p -Got a partial header 0x%x, RemainingLength %d \n", Stream, GotPartialType, RemainingLength);
                    switch (GotPartialType)
                    {
                    case HeaderZeroStartCode:
                        if (Player->PolicyValue(Playback, Stream, PolicyControlDataDetection) == PolicyValueApply)
                        {
                            // case 3 :  00 00 01 Control_Data code
                            if ((StoredPartialHeader[START_CODE_SIZE] == 0x00) && (StoredPartialHeader[5] == 0x01) && (StoredPartialHeader[6] == PES_START_CODE_CONTROL))
                            {
                                GotPartialType = HeaderControlStartCode;
                                GotPartialDesiredSize = PES_CONTROL_SIZE + 3;
                                RemainingData = RemainingData - GotPartialCurrentSize + 7;
                                RemainingLength = RemainingLength + GotPartialCurrentSize - 7;
                                MoveCurrentPartitionBoundary(-GotPartialCurrentSize + 7);
                                GotPartialCurrentSize = START_CODE_SIZE;
                                Loop = true;
                                break;
                            }
                        }

                        /* We step in there only if we have the pattern 00 00 01 00 00 01 then either a PES_PADDING_START_CODE or a PES start code.*/
                        /* For all other cases, 0 SC is considered as a generic SC.*/
                        if ((StoredPartialHeader[START_CODE_SIZE] == 0x00) && (StoredPartialHeader[5] == 0x01) &&
                            ((StoredPartialHeader[6] == PES_PADDING_START_CODE) || (IS_PES_START_CODE_VIDEO(StoredPartialHeader[6]))))
                        {
                            GotPartialType       = (StoredPartialHeader[6] == PES_PADDING_START_CODE) ? HeaderPaddingStartCode : HeaderPesStartCode;
                            GotPartialDesiredSize    = (StoredPartialHeader[6] == PES_PADDING_START_CODE) ? PES_PADDING_INITIAL_HEADER_SIZE : PES_INITIAL_HEADER_SIZE;

                            StoredPartialHeader     += 3;
                            GotPartialCurrentSize    = START_CODE_SIZE;
                        }
                        else
                        {
                            GotPartialDesiredSize    = START_CODE_SIZE + Stream->GetCollateTimeFrameParser()->RequiredPresentationLength(0x00);
                            GotPartialType       = HeaderGenericStartCode;
                        }
                        Loop                = true;
                        break;

//

                    case HeaderPesStartCode:
                        if (FoundPESHeader)
                            // StoredPartialHeader points to the start of PES SC
                        {
                            StoredPartialHeader = NextPartition->PartitionBase + NextPartition->PartitionSize - GotPartialCurrentSize + START_CODE_SIZE + StoreCodeOffset;
                        }

                        if (GotPartialCurrentSize >= PES_INITIAL_HEADER_SIZE)
                        {
                            // In case of PES SC cutting a generic SC, add the PartialDesiredSize associated to the cut generic SC
                            GotPartialDesiredSize   = PES_HEADER_SIZE(StoredPartialHeader) + CutGotPartialDesiredSize;
                        }

                        if (GotPartialCurrentSize < GotPartialDesiredSize)
                        {
                            Loop            = true;
                            break;
                        }

                        DiscardingData          = false;
                        GotPartialHeader        = false;
                        if (GotPartialCurrentSize > GotPartialDesiredSize)
                        {
                            // Store the extra copied bytes in temporary buffer CopyOfStoredPartialHeader to put them back in StoredPartialHeader
                            // after winding back of GotPartialCurrentSize.
                            memcpy(CopyOfStoredPartialHeader, StoredPartialHeader + GotPartialDesiredSize, (GotPartialCurrentSize - GotPartialDesiredSize));
                            SE_VERBOSE(group_collator_video, "Stream 0x%p More bytes than desired were copied\n", Stream);
                        }
                        MoveCurrentPartitionBoundary(-GotPartialCurrentSize);            // Wind the partition back to release the header
                        if (GotPartialCurrentSize > GotPartialDesiredSize)
                        {
                            // Put back the extra bytes to the end of StoredPartialHeader
                            AccumulateData((GotPartialCurrentSize - GotPartialDesiredSize), CopyOfStoredPartialHeader);
                        }

                        Status              = ReadPesHeader(StoredPartialHeader);

                        if (FoundPESHeader)
                        {
                            InjectedMoreThanRequestedByteNumber = GotPartialCurrentSize - GotPartialDesiredSize;
                            // Position RemainingData to the generic SC bytes following the PES header
                            RemainingData = RemainingData - CutGotPartialDesiredSize + START_CODE_SIZE + StoreCodeOffset - InjectedMoreThanRequestedByteNumber;
                            // Copy generic SC bytes cut by PES header to ES
                            AccumulateData(CutGotPartialDesiredSize - EXTRA_BYTES_TO_DETECT_SPANNING_START_CODE - START_CODE_SIZE
                                           - StoreCodeOffset + InjectedMoreThanRequestedByteNumber, RemainingData);
                            // Position RemainingData at the end of the cut SC
                            // as if we had detected the cut start code by call to FindNextStartCode() and accumulate corresponding data.
                            RemainingData += CutGotPartialDesiredSize - START_CODE_SIZE - StoreCodeOffset + InjectedMoreThanRequestedByteNumber
                                             - EXTRA_BYTES_TO_DETECT_SPANNING_START_CODE;

                            GotPartialType = CutPartialType;
                            // We cannot be here if ExtendDesiredSizeForGenericStartCode is not true
                            // It's now time to remove these 3 extra bytes
                            GotPartialDesiredSize = CutGotPartialDesiredSize - EXTRA_BYTES_TO_DETECT_SPANNING_START_CODE;
                            GotPartialCurrentSize = CutGotPartialCurrentSize + InjectedMoreThanRequestedByteNumber - 3;
                            CutGotPartialDesiredSize = 0;

                            ExtendDesiredSizeForGenericStartCode = false;
                            // Set GotPartialHeader back to true as RemainingData, GotPartialType and other variables related
                            // to the cut SC are now set back.
                            // Loop is set to true; there is no need to search for a new SC. We will just enter the loop again and process the cut SC.
                            GotPartialHeader = true;
                            FoundPESHeader = false;
                            Loop = true;
                            break;
                        }
                        if (Status != CollatorNoError)
                        {
                            Exit            = true;
                            break;
                        }

                        if (DiscardingData)
                        {
                            EmptyCurrentPartition();               // Dump any collected data in the current partition
                            InitializePartition();
                            DiscardingData      = false;
                        }

                        //
                        // Do we need to write the pts into the current partition
                        //

                        if ((NextPartition->PartitionSize == 0) && PlaybackTimeValid)
                        {
                            NextPartition->CodedFrameParameters.PlaybackTimeValid   = PlaybackTimeValid;
                            NextPartition->CodedFrameParameters.PlaybackTime        = PlaybackTime;
                            PlaybackTimeValid                                       = false;
                            NextPartition->CodedFrameParameters.DecodeTimeValid     = DecodeTimeValid;
                            NextPartition->CodedFrameParameters.DecodeTime          = DecodeTime;
                            DecodeTimeValid                                         = false;
                        }
                        break;

                    case HeaderPaddingStartCode:
                        Skipping            = PES_PADDING_SKIP(StoredPartialHeader);
                        GotPartialHeader        = false;
                        MoveCurrentPartitionBoundary(-GotPartialCurrentSize);            // Wind the partition back to release the header

                        break;

                    case HeaderControlStartCode:
                        if (FoundControlInHeader)
                        {
                            StoredPartialHeader = NextPartition->PartitionBase + NextPartition->PartitionSize - GotPartialCurrentSize + START_CODE_SIZE + StoreCodeOffset;
                        }
                        Status = ReadControlUserData(StoredPartialHeader, &NeedToRemoveControlFromESBuffer);
                        GotPartialHeader        = false;
                        if (Status == CollatorNotAControlData)
                        {
                            // Wind the partition back to remove control data from ES buffer
                            MoveCurrentPartitionBoundary(-GotPartialCurrentSize);
                            if (FoundControlInHeader)
                            {
                                FoundControlInHeader  = false;
                            }
                            break;
                        }

                        InjectedMoreThanRequestedByteNumber = GotPartialCurrentSize - GotPartialDesiredSize;
                        if (FoundControlInHeader)
                        {
                            if (NeedToRemoveControlFromESBuffer)
                            {
                                // Wind the partition back to remove control data from Es buffer
                                MoveCurrentPartitionBoundary(-GotPartialCurrentSize + StoreCodeOffset + START_CODE_SIZE);
                                RemainingData = RemainingData - CutGotPartialDesiredSize + START_CODE_SIZE + StoreCodeOffset - InjectedMoreThanRequestedByteNumber;
                                AccumulateData(CutGotPartialDesiredSize - START_CODE_SIZE - StoreCodeOffset + InjectedMoreThanRequestedByteNumber, RemainingData);
                                // Position RemainingData to the start code cut by the control data,
                                // as if we had detected this start code by call to FindNextStartCode().
                                RemainingData +=  CutGotPartialDesiredSize - START_CODE_SIZE - StoreCodeOffset + InjectedMoreThanRequestedByteNumber;
                                // Set variables to process start code that has been cut
                                GotPartialType = CutPartialType;
                                GotPartialDesiredSize = CutGotPartialDesiredSize;
                                GotPartialCurrentSize = CutGotPartialDesiredSize + InjectedMoreThanRequestedByteNumber ;
                                Loop = true;
                                GotPartialHeader = true;
                            }
                            FoundControlInHeader = false;
                            break;
                        }
                        // Search for SC being cut by control data, only if the 3 bytes after control data are available.
                        if (!FoundControlAtInjectionEnd)
                        {
                            SearchForStartCodeCutByControlData(InjectedMoreThanRequestedByteNumber);
                        }
                        if (StartCodeCutByControl)
                        {
                            break;
                        }

                        if (NeedToRemoveControlFromESBuffer)
                        {
                            // Wind the partition back to remove control data from ES buffer
                            MoveCurrentPartitionBoundary(-GotPartialCurrentSize);
                            // Reposition RemainingData just after the end of the control data. Update RemainingLength accordingly
                            if (!FoundControlAtInjectionEnd)
                            {
                                RemainingData -= GotPartialDesiredSize - PES_CONTROL_SIZE - InjectedMoreThanRequestedByteNumber;
                                RemainingLength += GotPartialDesiredSize - PES_CONTROL_SIZE + InjectedMoreThanRequestedByteNumber;
                            }
                            else
                            {
                                RemainingData = RemainingData - GotPartialCurrentSize + PES_CONTROL_SIZE;
                                RemainingLength = RemainingLength + GotPartialCurrentSize - PES_CONTROL_SIZE;
                            }
                        }
                        else
                        {
                            // Rewind the 3 bytes added to be able to check for SC cut by control data
                            RemainingData = RemainingData + PES_CONTROL_SIZE - GotPartialCurrentSize;
                            RemainingLength = RemainingLength + GotPartialCurrentSize - PES_CONTROL_SIZE;
                        }
                        break;

                    case HeaderGenericStartCode:
                        //
                        // Is it going to terminate a frame
                        //

                        Code                = StoredPartialHeader[3];

                        Stream->GetCollateTimeFrameParser()->PresentCollatedHeader(Code, (StoredPartialHeader + START_CODE_SIZE), &HeaderFlags);

                        NextPartition->FrameFlags   |= HeaderFlags;
                        BlockTerminate      = ((HeaderFlags & FrameParserHeaderFlagPartitionPoint) != 0);


                        // Accumulate all data in a partition if faced terminal start code
                        // by setting BlockTerminate = true
                        if (Code == Configuration.TerminalCode)
                        {
                            BlockTerminate = true;

                            // Accumulate the terminal start code
                            Status      = AccumulateStartCode(PackStartCode(NextPartition->PartitionSize - GotPartialCurrentSize, Code));
                            if (Status != CollatorNoError)
                            {
                                EmptyCurrentPartition();             // Dump any collected data in the current partition
                                InitializePartition();
                                Exit  = true;
                                break;
                            }
                        }

                        GotPartialHeader        = false;
                        if (BlockTerminate)
                        {
                            MoveCurrentPartitionBoundary(-GotPartialCurrentSize);            // Wind the partition back to release the header
                            memcpy(CopyOfStoredPartialHeader, StoredPartialHeader, GotPartialCurrentSize);
                            AccumulateOnePartition();
                            AccumulateData(GotPartialCurrentSize, CopyOfStoredPartialHeader);

                            PartitionPointSafeToOutputCount = PartitionPointUsedCount;
                            DiscardingData          = false;
                        }

                        //
                        // Accumulate it in any event
                        //
                        if (Code != Configuration.TerminalCode) // prevent double terminal code accumulation
                        {
                            Status      = AccumulateStartCode(PackStartCode(NextPartition->PartitionSize - GotPartialCurrentSize, Code));
                            if (Status != CollatorNoError)
                            {
                                EmptyCurrentPartition();               // Dump any collected data in the current partition
                                InitializePartition();
                                Exit    = true;
                                break;
                            }
                        }

                        //
                        // If we had a block terminate, then we need to check that there is sufficient
                        // room to accumulate the new frame, or we may end up copying flipping great
                        // wodges of data later.
                        //

                        if (BlockTerminate && (CodedFrameBufferFreeSpace < (LargestFrameSeen + MINIMUM_ACCUMULATION_HEADROOM)))
                        {
                            Status  = PartitionOutput(0);
                            if (Status != CollatorNoError)
                            {
                                SE_ERROR("Stream 0x%p -Output of partitions failed\n", Stream);
                                Exit    = true;
                                break;
                            }
                        }

                        break;
                    }
                }

                if (Exit)
                {
                    break;
                }

                if (Loop)
                {
                    continue;
                }
            }

            if (RemainingLength == 0)
            {
                break;
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
                break;
            }
        }

        //
        // Check for spanning header
        //
        // Check for a start code spanning, or in the first word
        // record the nature of the span in a counter indicating how many
        // bytes of the code are in the remaining data.

        if ((!FoundControlInHeader) && (!StartCodeCutByControl) && (!FoundPESHeader))
        {
            SpanningCount = ScanForSpanningStartcode(
                                NextPartition->PartitionBase, NextPartition->PartitionSize,
                                RemainingData, RemainingLength);

            // special case - startcode at the begining of RemainingData
            if (SpanningCount == 0 && RemainingLength >= 3 && memcmp(RemainingData, "\x00\x00\x01", 3) == 0)
            {
                SpanningCount               = START_CODE_SIZE;
                UseSpanningTime             = false;
                SpanningPlaybackTimeValid   = false;
                SpanningDecodeTimeValid     = false;
            }

            /** there is probably Spanning Code but we don't have it yet - accumulate and wait for rest of data */
            if (SpanningCount > RemainingLength)
            {
                Status = AccumulateData(RemainingLength, RemainingData);
                if ((Status != CollatorNoError) && (Status != CollatorWouldBlock))
                {
                    DiscardingData    = true;
                }
                RemainingLength                     -= RemainingLength;
                RemainingData                       += RemainingLength;
                break; // end
            }

            //
            // Check that if we have a spanning code, that the code is not to be ignored
            //

            if ((SpanningCount != 0) && inrange(RemainingData[SpanningCount - 1], Configuration.IgnoreCodesRangeStart, Configuration.IgnoreCodesRangeEnd))
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
                AccumulateData(SpanningCount, RemainingData);
                RemainingData           += SpanningCount;
                RemainingLength         -= SpanningCount;
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

                Status      = FindNextStartCode(&CodeOffset, 0xff);
                if (Status != CollatorNoError)
                {
                    //
                    // No start code, copy remaining data into buffer, and exit
                    //

                    Status  = AccumulateData(RemainingLength, RemainingData);

                    if (Status != CollatorWouldBlock)
                    {
                        RemainingLength   = 0;
                    }

                    if ((Status != CollatorNoError) && (Status != CollatorWouldBlock))
                    {
                        DiscardingData    = true;
                    }

                    break;
                }

                //
                // Got a start code accumulate up to it, and process
                //

                Status      = AccumulateData(CodeOffset + START_CODE_SIZE, RemainingData);

                if (Status != CollatorNoError)
                {
                    if (Status != CollatorWouldBlock)
                    {
                        DiscardingData    = true;
                    }

                    break;
                }

                RemainingLength                     -= CodeOffset + START_CODE_SIZE;
                RemainingData                       += CodeOffset + START_CODE_SIZE;
            }

            //
            // Now process the code, whether from spanning, or from search
            //

            GotPartialHeader        = true;
            GotPartialCurrentSize   = START_CODE_SIZE;

            // This is the pointer on ES buffer
            StoredPartialHeader     = NextPartition->PartitionBase + NextPartition->PartitionSize - GotPartialCurrentSize;
            Code                    = StoredPartialHeader[3];
            SE_VERBOSE(group_collator_video, "Stream 0x%p -Find code 0x%02x at offset %d \n", Stream, Code, CodeOffset);
        }
        else
        {
            if (FoundPESHeader)
            {
                Code = PES_START_CODE_VIDEO;
            }
            else
            {
                Code = PES_START_CODE_CONTROL;
                if (StartCodeCutByControl)
                {
                    Code = CutStartCode;
                }
            }
        }

        /* Handle 0 SC separately to check if a PES header is cutting the data here.*/
        /* If it's not the case, 0 SC will be then treated as a generic SC.*/
        if (Code == 0x00)
        {
            GotPartialType      = HeaderZeroStartCode;
            GotPartialDesiredSize   = ZERO_START_CODE_HEADER_SIZE;
        }
        else if ((Code == PES_START_CODE_CONTROL) &&
                 (Player->PolicyValue(Playback, Stream, PolicyControlDataDetection) == PolicyValueApply))
        {
            GotPartialType = HeaderControlStartCode;
            NeedToStepToSCTreatment = false;
            if (FoundControlInHeader)
            {
                GotPartialDesiredSize += PES_CONTROL_SIZE;
                GotPartialHeader        = true;
            }
            else
            {
                // Add extra bytes to be able to check for SC cut by control data
                GotPartialDesiredSize = PES_CONTROL_SIZE + EXTRA_BYTES_TO_DETECT_SPANNING_START_CODE;
            }
        }
        else if (IS_PES_START_CODE_VIDEO(Code))
        {
            if ((Code & Configuration.StreamIdentifierMask) == Configuration.StreamIdentifierCode)
            {
                GotPartialType      = HeaderPesStartCode;
                NeedToStepToSCTreatment = false;
                // If we detect a PES header cutting a SC header, add the PES header initial size to the first handled SC.
                // Once the PES header is treated, we get back to the previous detected SC and process it.
                if (FoundPESHeader)
                {
                    GotPartialDesiredSize += PES_INITIAL_HEADER_SIZE;
                    GotPartialHeader       = true;
                }
                else
                {
                    GotPartialDesiredSize = PES_INITIAL_HEADER_SIZE;
                }
            }
            else
            {
                // Not interested
                GotPartialHeader    = false;
                DiscardingData  = true;
            }
        }
        else if (Code == PES_PADDING_START_CODE)
        {
            GotPartialType      = HeaderPaddingStartCode;
            GotPartialDesiredSize   = PES_PADDING_INITIAL_HEADER_SIZE;
        }
        else if (DiscardingData)
        {
            // If currently seeking a pes header then ignore the last case of a generic header
            GotPartialHeader        = false;
            EmptyCurrentPartition();
        }
        else
        {
            // A generic start code
            GotPartialType      = HeaderGenericStartCode;
            GotPartialDesiredSize    = START_CODE_SIZE + Stream->GetCollateTimeFrameParser()->RequiredPresentationLength(Code);
            // Add extra bytes after RequiredPresentationLength to check if there is no PES header cutting the SC.
            if (Stream->GetCollateTimeFrameParser()->RequiredPresentationLength(Code) != 0)
            {
                GotPartialDesiredSize += EXTRA_BYTES_TO_DETECT_SPANNING_START_CODE;
                ExtendDesiredSizeForGenericStartCode = true;
            }
        }

        if ((StartCodeCutByControl) && (Player->PolicyValue(Playback, Stream, PolicyControlDataDetection) == PolicyValueApply))
        {
            StartCodeCutByControl = false;
            GotPartialHeader = true;
            GotPartialCurrentSize   = START_CODE_SIZE;
        }
    }



    if (DataLengthRemaining != NULL)
    {
        *DataLengthRemaining  = RemainingLength;
    }

    return Status;
}


////////////////////////////////////////////////////////////////////////////
//
//     Handle input, by scanning the start codes, and chunking the data
//     NOTE Most of the notes above also apply here.
//

CollatorStatus_t   Collator2_PesVideoHevc_c::ProcessInputBackward(
    unsigned int          DataLength,
    void             *Data,
    unsigned int         *DataLengthRemaining)
{
    CollatorStatus_t        Status;
    unsigned int            SpanningCount;
    unsigned int            CodeOffset;
    unsigned char           Code;
    unsigned int            AccumulationToCaptureCode;
    FrameParserHeaderFlag_t HeaderFlags;
    bool                    BlockTerminate;
    bool                    NeedToRemoveControlFromESBuffer;
    unsigned long long      Now = OS_GetTimeInMicroSeconds();

    st_relayfs_write_se(ST_RELAY_TYPE_PES_VIDEO_BUFFER, ST_RELAY_SOURCE_SE,
                        (unsigned char *)Data, DataLength, &Now);

    //
    // Initialize scan state
    //

    RemainingData       = (unsigned char *)Data;
    RemainingLength     = DataLength;
    Status      = CollatorNoError;
    Code = 0;

    while (RemainingLength != 0)
    {

        //
        // Allow any higher priority player processes in
        // NOTE On entry we have the partition lock
        //

        OS_UnLockMutex(&PartitionLock);
        OS_LockMutex(&PartitionLock);

        // Check if not in low power state
        if (Playback->IsLowPowerState())
        {
            // Stop processing data to speed-up low power enter procedure (bug 24248)
            break;
        }

        //
        // Do we have room to accumulate, if not we try passing on the
        // accumulated partitions and extending the buffer we have.
        //

        if (CodedFrameBufferFreeSpace < MINIMUM_ACCUMULATION_HEADROOM)
        {
            Status  = PartitionOutput(0);
            if (Status != CollatorNoError)
            {
                SE_ERROR("Output of partitions failed\n");
                break;
            }

            if (CodedFrameBufferFreeSpace < MINIMUM_ACCUMULATION_HEADROOM)
            {
                if (!NonBlockingInput)
                {
                    SE_FATAL("About to return CollatorWouldBlock when it is OK to block - Probable implementation error\n");
                }

                Status  = CollatorWouldBlock;
                break;
            }
        }

        //
        // Check for spanning header
        //

        if (!StartCodeCutByControl)
        {
            SpanningCount = ScanForSpanningStartcode(
                                RemainingData, RemainingLength,
                                NextPartition->PartitionBase, NextPartition->PartitionSize);

            SpanningCount   = min(SpanningCount, NextPartition->PartitionSize);

            //
            // Check that if we have a spanning code, that the code is not to be ignored
            //

            if ((SpanningCount != 0) &&
                inrange(RemainingData[SpanningCount - 1], Configuration.IgnoreCodesRangeStart, Configuration.IgnoreCodesRangeEnd))
            {
                SpanningCount       = 0;
            }

            //
            // Handle a spanning start code
            //

            if (SpanningCount != 0)
            {
                AccumulationToCaptureCode   = (START_CODE_SIZE - SpanningCount);
            }

            //
            // Handle search for next start code
            //

            else
            {
                //
                // Get a new start code
                //

                Status      = FindPreviousStartCode(&CodeOffset);

                if (Status != CollatorNoError)
                {
                    //
                    // No start code, copy remaining data into buffer, and exit
                    //

                    Status  = AccumulateData(RemainingLength, RemainingData);

                    if (Status != CollatorWouldBlock)
                    {
                        RemainingLength   = 0;
                    }

                    if ((Status != CollatorNoError) && (Status != CollatorWouldBlock))
                    {
                        DiscardingData    = true;
                    }

                    break;
                }

                //
                // Got a start code accumulate up to it, and process
                //

                AccumulationToCaptureCode   = (RemainingLength - CodeOffset);
            }

            //
            // Now process the code, whether from spanning, or from search
            //

            Status  = AccumulateData(AccumulationToCaptureCode, RemainingData + RemainingLength - AccumulationToCaptureCode);
            if (Status != CollatorNoError)
            {
                if (Status == CollatorWouldBlock)
                {
                    break;
                }

                DiscardingData  = true;
            }

            RemainingLength     -= AccumulationToCaptureCode;
            Code             = NextPartition->PartitionBase[3];
        }

        if (IS_PES_START_CODE_VIDEO(Code) ||
            (Code == PES_PADDING_START_CODE))
        {
            StartCodeCutByControl = false;
            DiscardingData  = (Code == PES_PADDING_START_CODE)                          ||  // It's a padding packet
                              (NextPartition->PartitionSize < PES_INITIAL_HEADER_SIZE)              ||  // There isn't a full pes header
                              (NextPartition->PartitionSize < PES_HEADER_SIZE(NextPartition->PartitionBase));

            if (!DiscardingData)
            {
                Status      = ReadPesHeader(NextPartition->PartitionBase);
                if (Status != CollatorNoError)
                {
                    DiscardingData    = true;
                }
            }

            if (!DiscardingData)
            {
                //
                // Skip the packet header
                //

                MoveCurrentPartitionBoundary(-PES_HEADER_SIZE(NextPartition->PartitionBase));

                //
                // Do we have a PTS to write
                //

                if (PlaybackTimeValid &&
                    ((PartitionPointUsedCount != PartitionPointMarkerCount) || (PartitionPointUsedCount == 0)))
                {
                    // Make sure the read header function did not pollute the new partition
                    NextPartition->CodedFrameParameters.PlaybackTimeValid               = false;
                    NextPartition->CodedFrameParameters.DecodeTimeValid                 = false;

                    PartitionPoints[PartitionPointMarkerCount].CodedFrameParameters.PlaybackTimeValid   = PlaybackTimeValid;
                    PartitionPoints[PartitionPointMarkerCount].CodedFrameParameters.PlaybackTime    = PlaybackTime;
                    PartitionPoints[PartitionPointMarkerCount].CodedFrameParameters.DecodeTimeValid = DecodeTimeValid;
                    PartitionPoints[PartitionPointMarkerCount].CodedFrameParameters.DecodeTime      = DecodeTime;

                    PlaybackTimeValid                       = false;
                    DecodeTimeValid                     = false;
                }

                //
                // Record the state
                //

                PartitionPointMarkerCount       = PartitionPointUsedCount;
            }
        }
        else if (Code == PES_START_CODE_CONTROL)
        {
            // A control data start code
            StoredPartialHeader = NextPartition->PartitionBase + NextPartition->PartitionSize - PES_CONTROL_SIZE;
            Status = ReadControlUserData(StoredPartialHeader, &NeedToRemoveControlFromESBuffer);
            if (Status == CollatorNotAControlData)
            {
                MoveCurrentPartitionBoundary(-PES_CONTROL_SIZE);   // Wind the partition back to remove control data from Es buffer
                break;
            }
            // Check that the control data was not cutting a start code
            // case 1 :  00 Control_Data 00 01 code
            if ((RemainingLength > 0) && (DataLength > PES_CONTROL_SIZE + 3))
            {
                if ((RemainingData[RemainingLength] == 0) && (RemainingData[RemainingLength + PES_CONTROL_SIZE] == 0) &&
                    (RemainingData[RemainingLength + PES_CONTROL_SIZE + 1] == 1))
                {
                    Code = RemainingData[RemainingLength + PES_CONTROL_SIZE + 2] ;
                    if (!inrange(Code, Configuration.IgnoreCodesRangeStart, Configuration.IgnoreCodesRangeEnd))
                    {
                        // Remove control data from ES buffer
                        MoveCurrentPartitionBoundary(-PES_CONTROL_SIZE);
                        AccumulateData(1, RemainingData + RemainingLength - 1);
                        RemainingLength -= 1;
                        StartCodeCutByControl = true;
                        break;
                    }
                }
            }
            /*
            else if ((NextPartition->PartitionSize < PES_CONTROL_SIZE + 3) && (PartitionPointUsedCount > 1)
                && (RemainingLength > 0) && (!DiscardingData))
                Need to handle this case later on. It's not possible for now as we don't concatenate
                two contiguous, not frame aligned injections (we don't know the size of the last memcpy done
                with previous injection on the remaining data without SC).
                */

            // case 2 :  00 00 Control_Data 01 code
            if ((RemainingLength > 1) && (DataLength > PES_CONTROL_SIZE + 3))
            {
                if ((RemainingData[RemainingLength - 1] == 0) && (RemainingData[RemainingLength] == 0) &&
                    (RemainingData[RemainingLength + PES_CONTROL_SIZE] == 1))
                {
                    Code = RemainingData[RemainingLength + PES_CONTROL_SIZE + 1] ;
                    if (!inrange(Code, Configuration.IgnoreCodesRangeStart, Configuration.IgnoreCodesRangeEnd))
                    {
                        MoveCurrentPartitionBoundary(-PES_CONTROL_SIZE);
                        AccumulateData(2, RemainingData + RemainingLength - 2);
                        RemainingLength -= 2;
                        StartCodeCutByControl = true;
                        break;
                    }
                }
            }
            /*
            else if ((NextPartition->PartitionSize < PES_CONTROL_SIZE + 2) && (PartitionPointUsedCount > 1)
                && (RemainingLength > 1) && (!DiscardingData))
                Need to handle this case later on. It's not possible for now as we don't concatenate
                two contiguous, not frame aligned injections (we don't know the size of the last memcpy done
                with previous injection on the remaining data without SC).
                */
            // case 3 :  00 00 01 Control_Data code
            if ((RemainingLength > 2) && (DataLength > PES_CONTROL_SIZE + 3))
            {
                if ((RemainingData[RemainingLength - 2] == 0) && (RemainingData[RemainingLength - 1] == 0) &&
                    (RemainingData[RemainingLength] == 1))
                {
                    Code = RemainingData[RemainingLength + PES_CONTROL_SIZE] ;
                    if (!inrange(Code, Configuration.IgnoreCodesRangeStart, Configuration.IgnoreCodesRangeEnd))
                    {
                        MoveCurrentPartitionBoundary(-PES_CONTROL_SIZE);
                        AccumulateData(3, RemainingData + RemainingLength - 3);
                        RemainingLength -= 3;
                        StartCodeCutByControl = true;
                        break;
                    }
                }
            }
            /*
            else if ((NextPartition->PartitionSize < PES_CONTROL_SIZE + 1) && (PartitionPointUsedCount > 1)
                && (RemainingLength > 2) && (!DiscardingData))
                Need to handle this case later on. It's not possible for now as we don't concatenate
                two contiguous, not frame aligned injections (we don't know the size of the last memcpy done
                with previous injection on the remaining data without SC).
                */
            // Remove the control data from ES buffer only if control data was not a break one. In case of break, it's already removed.
            if (NeedToRemoveControlFromESBuffer)
            {
                MoveCurrentPartitionBoundary(-PES_CONTROL_SIZE);
            }
        }
        else
        {
            //
            // A generic start code
            //
            StartCodeCutByControl = false;
            Status  = AccumulateStartCode(PackStartCode(NextPartition->PartitionSize, Code));
            if (Status != CollatorNoError)
            {
                DiscardingData  = true;

                EmptyCurrentPartition();               // Dump any collected data in the current partition
                break;
            }

            //
            // Is it going to terminate a frame
            //

            HeaderFlags         = 0;
            if (NextPartition->PartitionSize > (START_CODE_SIZE + Stream->GetCollateTimeFrameParser()->RequiredPresentationLength(Code)))
            {
                Stream->GetCollateTimeFrameParser()->PresentCollatedHeader(Code, (NextPartition->PartitionBase + START_CODE_SIZE), &HeaderFlags);
            }

            NextPartition->FrameFlags   |= HeaderFlags;
            BlockTerminate       = (HeaderFlags & FrameParserHeaderFlagPartitionPoint) != 0;
            if (BlockTerminate)
            {
                //
                // Adjust control data and accumulate the partition
                //

                PartitionPointSafeToOutputCount = PartitionPointMarkerCount;
                PartitionPointMarkerCount   = PartitionPointUsedCount;

                AccumulateOnePartition();

                //
                // We need to check that there is sufficient room to accumulate
                // the new frame, or we may end up copying flipping great
                // wodges of data later.
                //

                if (((HeaderFlags & FrameParserHeaderFlagConfirmReversiblePoint) != 0) ||
                    (CodedFrameBufferFreeSpace < (LargestFrameSeen + MINIMUM_ACCUMULATION_HEADROOM)))
                {
// Too fullness, how do we detect, and handle this ?
// also too many partitions in the table
                    Status  = PartitionOutput(0);
                    if (Status != CollatorNoError)
                    {
                        SE_ERROR("Output of partitions failed\n");
                        break;
                    }
                }
            }
        }
    }

    if (DataLengthRemaining != NULL)
    {
        *DataLengthRemaining  = RemainingLength;
    }

    return Status;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The overlaod of the accumulate partition function,
//  adds initialization of pts info to new partition
//

void   Collator2_PesVideoHevc_c::AccumulateOnePartition(void)
{
    Collator2_Pes_c::AccumulateOnePartition();

    DiscardingData  = false;
    GotPartialHeader    = false;
    Skipping        = 0;

    //
    // at this point we sit (approximately) between frames and should update the PTS/DTS with the values
    // last extracted from the PES header. UseSpanningTime will (or at least should) be true when the
    // frame header spans two PES packets, at this point the frame started in the previous packet and
    // should therefore use the older PTS.
    //
    if (UseSpanningTime)
    {
        NextPartition->CodedFrameParameters.PlaybackTimeValid   = SpanningPlaybackTimeValid;
        NextPartition->CodedFrameParameters.PlaybackTime    = SpanningPlaybackTime;
        SpanningPlaybackTimeValid               = false;
        NextPartition->CodedFrameParameters.DecodeTimeValid = SpanningDecodeTimeValid;
        NextPartition->CodedFrameParameters.DecodeTime      = SpanningDecodeTime;
        SpanningDecodeTimeValid                 = false;
    }
    else
    {
        NextPartition->CodedFrameParameters.PlaybackTimeValid   = PlaybackTimeValid;
        NextPartition->CodedFrameParameters.PlaybackTime    = PlaybackTime;
        PlaybackTimeValid                   = false;
        NextPartition->CodedFrameParameters.DecodeTimeValid = DecodeTimeValid;
        NextPartition->CodedFrameParameters.DecodeTime      = DecodeTime;
        DecodeTimeValid                     = false;
    }
}
