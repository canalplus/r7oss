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

// /////////////////////////////////////////////////////////////////////////
/// \class Collator_PesAudio_c
///
/// Specialized PES collator implementing audio specific features.
///
/// None of the common audio standards use MPEG start codes to demark
/// frame boundaries. Instead, once the stream has been de-packetized
/// we must perform a limited form of frame analysis to discover the
/// frame boundaries.
///
/// This class provides a framework on which to base that frame
/// analysis. Basically this class provides most of the machinary to
/// manage buffers but leaves the sub-class to implement
/// Collator_PesAudio_c::ScanForSyncWord
/// and Collator_PesAudio_c::GetFrameLength.
///
/// \todo Currently this class does not honour PES padding (causing
///       accumulated data to be spuriously discarded when playing back
///       padded streams).
///

// /////////////////////////////////////////////////////////////////////
//
//  Include any component headers

#include "st_relayfs_se.h"
#include "collator_pes_audio.h"

////////////////////////////////////////////////////////////////////////////
/// \fn CollatorStatus_t Collator_PesAudio_c::FindNextSyncWord( int *CodeOffset )
///
/// Scan the input until an appropriate synchronization sequence is found.
///
/// If the encoding format does not have any useable synchronization sequence
/// this method should set CodeOffset to zero and return CollatorNoError.
///
/// In addition to scanning the input found in Collator_PesAudio_c::RemainingElementaryData
/// this method should also examine Collator_PesAudio_c::PotentialFrameHeader to see
/// if there is a synchronization sequence that spans blocks. If such a sequence
/// if found this is indicated by providing a negative offset.
///
/// \return Collator status code, CollatorNoError indicates success.
///

////////////////////////////////////////////////////////////////////////////
/// \fn CollatorStatus_t Collator_PesAudio_c::DecideCollatorNextStateAndGetLength( unsigned int *FrameLength )
///
/// Examine the frame header pointed to by Collator_PesAudioEAc3_c::StoredFrameHeader
/// and determine how many bytes should be accumulated. Additionally this method
/// sets the collator state.
///
/// For trivial bitstreams where the header identifies the exact length of the frame
/// (e.g. MPEG audio) the state is typically set to GotCompleteFrame to indicate that
/// the previously accumulated frame should be emitted.
///
/// More complex bitstreams may require secondary analysis (by a later call to
/// this method) and these typically use either the ReadSubFrame state if they want to
/// accumulate data prior to subsequent analysis or the SkipSubFrame state to discard
/// data prior to subsequent analysis.
///
/// Note that even for trivial bitstreams the collator must use the ReadSubFrame state
/// for the first frame after a glitch since there is nothing to emit.
///
/// If the encoding format does not have any useable synchronization sequence
/// and therefore no easy means to calculate the frame length then an arbitary
/// chunk size should be selected. The choice of chunk size will depend on the
/// level of compresion achieved by the encoder but in general 1024 bytes would
/// seem appropriate for all but PCM data.
///
/// \return Collator status code, CollatorNoError indicates success.
///

////////////////////////////////////////////////////////////////////////////
///
///
Collator_PesAudio_c::Collator_PesAudio_c(unsigned int frameheaderlength)
    : Collator_Pes_c()
    , PotentialFrameHeader()
    , CollatorState(SeekingSyncWord)
    , PassPesPrivateDataToElementaryStreamHandler(true)
    , DiscardPesPacket(false)
    , ReprocessAccumulatedDataDuringErrorRecovery(true)
    , AlreadyHandlingMissingNextFrameHeader(false)
    , PotentialFrameHeaderLength(0)
    , StoredFrameHeader(NULL)
    , RemainingElementaryLength(0)
    , RemainingElementaryData(NULL)
    , RemainingElementaryOrigin(NULL)
    , FrameHeaderLength(frameheaderlength)
    , Framecount(0)
    , mControlDataDetectionState(SeekingControlData)
    , mControlDataTrailingStartCodeBytes(0)
    , mGotPartialControlDataBytes(0)
    , mControlDataRemainingData(NULL)
    , mControlDataRemainingLength(0)
    , mStoredControlData()
    , PesPayloadRemaining(0)
    , TrailingStartCodeBytes(0)
    , GotPartialFrameHeaderBytes(0)
    , FramePayloadRemaining(0)
    , AccumulatedFrameReady(false)
    , NextPlaybackTimeValid(false)
    , NextPlaybackTime(0)
    , NextDecodeTimeValid(false)
    , NextDecodeTime(0)
    , NextPesADInfo()
{
    SetGroupTrace(group_collator_audio);
}

// /////////////////////////////////////////////////////////////////////////
//
// (Protected) The accumulate data into the buffer function
//
CollatorStatus_t   Collator_PesAudio_c::AccumulateData(
    unsigned int              Length,
    unsigned char            *Data)
{
    CollatorStatus_t status = Collator_Base_c::AccumulateData(Length, Data);

    SE_DEBUG(group_collator_audio, ">> %d bytes from %p [Total %d] \n", Length, Data, AccumulatedDataSize);
    return status;
}



////////////////////////////////////////////////////////////////////////////
///
/// Scan the input until an appropriate synchronization sequence is found.
///
/// Scans any ::RemainingElementaryData searching for a
/// synchronization sequence using ::FindNextSyncWord,
/// a pure virtual method provided by sub-classes.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudio_c::SearchForSyncWord(void)
{
    CollatorStatus_t Status;
    int CodeOffset;

    SE_DEBUG(group_collator_audio, ">><<\n");
    Status = FindNextSyncWord(&CodeOffset);

    if (Status == CollatorNoError)
    {
        SE_DEBUG(group_collator_audio, "Tentatively locked to synchronization sequence (at %d)\n", CodeOffset);
        // switch state
        CollatorState = GotSynchronized;
        GotPartialFrameHeaderBytes = 0;

        if (CodeOffset >= 0)
        {
            // discard any data leading up to the start code
            RemainingElementaryData += CodeOffset;
            RemainingElementaryLength -= CodeOffset;
        }
        else
        {
            SE_DEBUG(group_collator_audio, "Synchronization sequence spans multiple PES packets\n");
            // accumulate any data from the old block
            Status = AccumulateData(-1 * CodeOffset,
                                    PotentialFrameHeader + PotentialFrameHeaderLength + CodeOffset);

            if (Status != CollatorNoError)
            {
                SE_DEBUG(group_collator_audio, "Cannot accumulate data #1 (%d)\n", Status);
            }

            GotPartialFrameHeaderBytes += (-1 * CodeOffset);
        }

        PotentialFrameHeaderLength = 0;
    }
    else
    {
        // copy the last few bytes of the frame into PotentialFrameHeader (so that FindNextSyncWord
        // can return a negative CodeOffset if the synchronization sequence spans blocks)
        if (RemainingElementaryLength < FrameHeaderLength)
        {
            if (PotentialFrameHeaderLength + RemainingElementaryLength > FrameHeaderLength)
            {
                // shuffle the existing potential frame header downwards
                unsigned int BytesToKeep = FrameHeaderLength - RemainingElementaryLength;
                unsigned int ShuffleBy = PotentialFrameHeaderLength - BytesToKeep;
                memmove(PotentialFrameHeader, PotentialFrameHeader + ShuffleBy, BytesToKeep);
                PotentialFrameHeaderLength = BytesToKeep;
            }

            memcpy(PotentialFrameHeader + PotentialFrameHeaderLength,
                   RemainingElementaryData,
                   RemainingElementaryLength);
            PotentialFrameHeaderLength += RemainingElementaryLength;
        }
        else
        {
            memcpy(PotentialFrameHeader,
                   RemainingElementaryData + RemainingElementaryLength - FrameHeaderLength,
                   FrameHeaderLength);
            PotentialFrameHeaderLength = FrameHeaderLength;
        }

        SE_DEBUG(group_collator_audio, "Discarded %d bytes while searching for synchronization sequence\n",
                 RemainingElementaryLength);
        RemainingElementaryLength = 0;
        // we have contained the 'error' and don't want to propagate it
        Status = CollatorNoError;
    }

    return Status;
}

////////////////////////////////////////////////////////////////////////////
///
/// Accumulate incoming data until we have the parseable frame header.
///
/// Accumulate data until we have ::FrameHeaderLength
/// bytes stashed away. At this point there is sufficient data accumulated
/// to determine how many bytes will pass us by before the next frame header
/// is expected.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudio_c::ReadPartialFrameHeader(void)
{
    CollatorStatus_t Status;
    unsigned int BytesNeeded, BytesToRead, FrameLength;
    unsigned int OldPartialFrameHeaderBytes, OldAccumulatedDataSize;
    CollatorState_t OldCollatorState;
    //
    SE_DEBUG(group_collator_audio, ">><<\n");
    OldPartialFrameHeaderBytes = GotPartialFrameHeaderBytes;
    OldAccumulatedDataSize = AccumulatedDataSize;
    //
    BytesNeeded = FrameHeaderLength - GotPartialFrameHeaderBytes;
    BytesToRead = min(RemainingElementaryLength, BytesNeeded);
    Status = AccumulateData(BytesToRead, RemainingElementaryData);

    if (Status == CollatorNoError)
    {
        GotPartialFrameHeaderBytes += BytesToRead;
        RemainingElementaryData += BytesToRead;
        RemainingElementaryLength -= BytesToRead;
        SE_DEBUG(group_collator_audio, "BytesNeeded %d; BytesToRead %d\n", BytesNeeded, BytesToRead);

        if (BytesNeeded == BytesToRead)
        {
            //
            // Check for inconsistent class state, dump some diagnostics and
            // attempt error correction.
            //
            if (AccumulatedDataSize < FrameHeaderLength)
            {
                SE_ERROR("Internal error; GotPartialFrameHeaderBytes (%d) inconsistent with AccumulatedDataSize (%d)\n",
                         OldPartialFrameHeaderBytes, OldAccumulatedDataSize);
                // Correct the inconsistancy and give up in this frame
                GotPartialFrameHeaderBytes = 0;
                return HandleMissingNextFrameHeader();
            }

            //
            // We've got the whole header, examine it and change state
            //
            StoredFrameHeader = BufferBase + (AccumulatedDataSize - FrameHeaderLength);
            SE_DEBUG(group_collator_audio, "Got entire frame header packet\n");
            //report_dump_hex( severity_debug, StoredFrameHeader, FrameHeaderLength, 32, 0);
            OldCollatorState = CollatorState;
            Status = DecideCollatorNextStateAndGetLength(&FrameLength);

            if (Status != CollatorNoError)
            {
                SE_DEBUG(group_collator_audio, "Badly formed frame header; seeking new frame header\n");
                return HandleMissingNextFrameHeader();
            }

            if (FrameLength == 0)
            {
                // The LPCM collator needs to do this in order to get a frame evicted before
                // accumulating data from the PES private data area into the frame. The only
                // way it can do this is by reporting a zero length frame and updating some
                // internal state variables. On the next call it will report a non-zero value
                // (i.e. we won't loop forever accumulating no data).
                SE_DEBUG(group_collator_audio, "Sub-class reported unlikely (but potentially legitimate) frame length (%d)\n", FrameLength);
            }

            if (FrameLength > MaximumCodedFrameSize)
            {
                SE_ERROR("Sub-class reported absurd frame length (%d)\n", FrameLength);
                return HandleMissingNextFrameHeader();
            }

            // this is the number of bytes we must absorb before switching state to SeekingFrameEnd.
            // if the value is negative then we've already started absorbing the subsequent frame
            // header.
            FramePayloadRemaining = FrameLength - FrameHeaderLength;

            if (CollatorState == GotCompleteFrame)
            {
                AccumulatedFrameReady = true;
                //
                // update the coded frame parameters using the parameters calculated the
                // last time we saw a frame header.
                //
                if (false == CodedFrameParameters->PlaybackTimeValid)
                {
                    CodedFrameParameters->PlaybackTimeValid = NextPlaybackTimeValid;
                    CodedFrameParameters->PlaybackTime = NextPlaybackTime;
                }
                if (false == CodedFrameParameters->DecodeTimeValid)
                {
                    CodedFrameParameters->DecodeTimeValid = NextDecodeTimeValid;
                    CodedFrameParameters->DecodeTime = NextDecodeTime;
                }
                //
                //AD related parameter filled to CodedFrameParameters structure for
                //passing this info to neighbour module(frame parser).
                //
                memcpy(&CodedFrameParameters->ADMetaData, &NextPesADInfo, sizeof(NextPesADInfo));
            }
            else if (CollatorState == SkipSubFrame)
            {
                /* discard the accumulated frame header */
                AccumulatedDataSize -= FrameHeaderLength;
            }

            if (CollatorState == GotCompleteFrame || OldCollatorState == GotSynchronized)
            {
                //
                // at this point we have discovered a frame header and need to attach a time to it.
                // we can choose between the normal stamp (the stamp of the current PES packet) or the
                // spanning stamp (the stamp of the previous PES packet). Basically if we have accumulated
                // a greater number of bytes than our current offset into the PES packet then we want to
                // use the spanning time.
                //
                bool WantSpanningTime = GetOffsetIntoPacket() < (int) GotPartialFrameHeaderBytes;

                if (WantSpanningTime && !UseSpanningTime)
                {
                    SE_ERROR("Wanted to take the spanning time but this was not available.");
                    WantSpanningTime = false;
                }

                if (WantSpanningTime)
                {
                    NextPlaybackTimeValid = SpanningPlaybackTimeValid;
                    NextPlaybackTime  = SpanningPlaybackTime;
                    SpanningPlaybackTimeValid = false;
                    NextDecodeTimeValid   = SpanningDecodeTimeValid;
                    NextDecodeTime    = SpanningDecodeTime;
                    SpanningDecodeTimeValid = false;
                    UseSpanningTime       = false;
                }
                else
                {
                    NextPlaybackTimeValid = PlaybackTimeValid;
                    NextPlaybackTime  = PlaybackTime;
                    PlaybackTimeValid = false;
                    NextDecodeTimeValid   = DecodeTimeValid;
                    NextDecodeTime    = DecodeTime;
                    DecodeTimeValid   = false;
                    //Audio Description related value stored to update in CodedFrameParameters
                    NextPesADInfo.NextADFadeValue = AudioDescriptionInfo.ADFadeValue;
                    NextPesADInfo.NextADPanValue = AudioDescriptionInfo.ADPanValue;
                    NextPesADInfo.NextADGainCenter = AudioDescriptionInfo.ADGainCenter ;
                    NextPesADInfo.NextADGainFront = AudioDescriptionInfo.ADGainFront ;
                    NextPesADInfo.NextADGainSurround = AudioDescriptionInfo.ADGainSurround;
                    NextPesADInfo.NextADValidFlag = AudioDescriptionInfo.ADValidFlag;
                    NextPesADInfo.NextADInfoAvailable = AudioDescriptionInfo.ADInfoAvailable;
                }
            }

            // switch states and absorb the packet
            SE_DEBUG(group_collator_audio, "Discovered frame header (frame length %d bytes)\n", FrameLength);
        }
    }
    else
    {
        SE_DEBUG(group_collator_audio, "Cannot accumulate data #3 (%d)\n", Status);
    }

    return Status;
}


////////////////////////////////////////////////////////////////////////////
///
/// Handle losing lock on the frame headers.
///
/// This function is called to handle the data that was spuriously accumulated
/// when the frame header was badly parsed.
///
/// In principle this function is quite simple. We allocate a new accumulation buffer and
/// use the currently accumulated data is the data source to run the elementary stream
/// state machine. There is however a little extra logic to get rid of recursion.
/// Specificially we change the error handling behaviour if this method is re-entered
/// so that there error is reported back to the already executing copy of the method.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudio_c::HandleMissingNextFrameHeader(void)
{
    //
    // Mark the collator as having lost frame lock.
    // Yes! We really do want to do this before the re-entry checks.
    //
    CollatorState = SeekingSyncWord; // we really do want to do this before the re-entry checks
    AccumulatedFrameReady = false;

    //
    // Check for re-entry
    //

    if (AlreadyHandlingMissingNextFrameHeader)
    {
        SE_DEBUG(group_collator_audio, "Re-entered the error recovery handler, initiating stack unwind\n");
        return CollatorUnwindStack;
    }

    //
    // Check whether the sub-class wants trivial or aggressive error recovery
    //
    Stream->Statistics().CollatorAudioElementrySyncLostCount++;

    if (!ReprocessAccumulatedDataDuringErrorRecovery)
    {
        DiscardAccumulatedData();
        return CollatorNoError;
    }

    //
    // Remember the original elementary stream pointers for when we return to 'normal' processing.
    //
    unsigned char *OldRemainingElementaryOrigin = RemainingElementaryOrigin;
    unsigned char *OldRemainingElementaryData   = RemainingElementaryData;
    unsigned int OldRemainingElementaryLength   = RemainingElementaryLength;
    //
    // Take ownership of the already accumulated data
    //
    Buffer_t ReprocessingDataBuffer = CodedFrameBuffer;
    unsigned char *ReprocessingData = BufferBase;
    unsigned int ReprocessingDataLength = AccumulatedDataSize;
    ReprocessingDataBuffer->SetUsedDataSize(ReprocessingDataLength);
    BufferStatus_t BufStatus = ReprocessingDataBuffer->ShrinkBuffer(max(ReprocessingDataLength, 1));
    if (BufStatus != BufferNoError)
    {
        SE_ERROR("Failed to shrink the reprocessing buffer to size (%08x)\n", BufStatus);
        // not fatal - we're merely wasting memory
    }

    // At the time of writing GetNewBuffer() doesn't check for leaks. This is good because otherwise
    // we wouldn't have transfer the ownership of the ReprocessingDataBuffer by making this call.
    CollatorStatus_t Status = GetNewBuffer();
    if (Status != CollatorNoError)
    {
        SE_ERROR("Cannot get new buffer during error recovery\n");
        return CollatorError;
    }

    //
    // Remember that we are re-processing the previously accumulated elementary stream
    //
    AlreadyHandlingMissingNextFrameHeader = true;

    //
    // WARNING: From this point on we own the ReprocessingDataBuffer, have set the recursion avoidance
    //          marker and may have damaged the RemainingElementaryData pointer. There should be no
    //          short-circuit exit paths used after this point otherwise we risk avoiding the clean up
    //          at the bottom of the method.
    //

    while (ReprocessingDataLength > 1)
    {
        //
        // Remove the first byte from the recovery buffer (to avoid detecting again the same start code).
        //
        ReprocessingData += 1;
        ReprocessingDataLength -= 1;
        //
        // Search for a start code in the reprocessing data. This allows us to throw away data that we
        // know will never need reprocessing which makes the recursion avoidance code more efficient.
        //
        RemainingElementaryOrigin = ReprocessingData;
        RemainingElementaryData   = ReprocessingData;
        RemainingElementaryLength = ReprocessingDataLength;
        PotentialFrameHeaderLength = 0; // ensure no (now voided) historic data is considered by sub-class
        //
        // Process the elementary stream
        //
        Status = HandleElementaryStream(ReprocessingDataLength, ReprocessingData);

        if (CollatorNoError == Status)
        {
            SE_DEBUG(group_collator_audio, "Error recovery completed, returning to normal processing\n");
            // All data consumed and stored in the subsequent accumulation buffer
            break; // Success will propagate when we return Status
        }
        else if (CollatorUnwindStack == Status)
        {
            SE_DEBUG(group_collator_audio, "Stack unwound successfully, re-trying error recovery\n");

            // Double check that we really are acting upon the reprocessing buffer
            if ((AccumulatedDataSize > ReprocessingDataLength) ||
                (ReprocessingData + ReprocessingDataLength !=
                 RemainingElementaryData + RemainingElementaryLength))
            {
                SE_ERROR("Internal failure during error recovery, returning to normal processing\n");
                break; // Failure will propagate when we return Status
            }

            // We found a frame header (or potentially a couple of valid frame)
            // but lost lock again... let's restart from where we got to
            ReprocessingData = RemainingElementaryData - AccumulatedDataSize;
            ReprocessingDataLength = RemainingElementaryLength + AccumulatedDataSize;
            AccumulatedDataSize = 0;
            continue;
        }
        else
        {
            SE_ERROR("Error handling elementary stream during error recovery\n");
            break; // Failure will propagate when we return Status
        }
    }

    //
    // Free the buffer we just consumed and restore the original elementary stream pointers
    //
    RemainingElementaryOrigin = OldRemainingElementaryOrigin;
    RemainingElementaryData   = OldRemainingElementaryData;
    RemainingElementaryLength = OldRemainingElementaryLength;
    (void) ReprocessingDataBuffer->DecrementReferenceCount(IdentifierCollator);
    AlreadyHandlingMissingNextFrameHeader = false;
    return Status;
}


////////////////////////////////////////////////////////////////////////////
///
/// Accumulate data until we reach the next frame header.
///
/// The function has little or no intelligence. Basically it will squirrel
/// away data until Collator_PesAudio_c::GotPartialFrameHeaderBytes reaches
/// Collator_PesAudio_c::FrameHeaderLength and then set
/// Collator_PesAudio_c::GotPartialFrameHeader.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudio_c::ReadFrame(void)
{
    CollatorStatus_t Status;
    unsigned int BytesToRead;
    //
    SE_DEBUG(group_collator_audio, ">><<\n");

    //

    // if the FramePayloadRemaining is -ve then we have no work to do except
    // record the fact that we've already accumulated part of the next frame header
    if (FramePayloadRemaining < 0)
    {
        GotPartialFrameHeaderBytes = -FramePayloadRemaining;
        CollatorState = SeekingFrameEnd;
        return CollatorNoError;
    }

    // FramePayloadRemaining is postive so no worry about casting
    BytesToRead = min((unsigned int)FramePayloadRemaining, RemainingElementaryLength);

    if (CollatorState == ReadSubFrame)
    {
        Status = AccumulateData(BytesToRead, RemainingElementaryData);

        if (Status != CollatorNoError)
        {
            SE_DEBUG(group_collator_audio, "Cannot accumulate data #4 (%d)\n", Status);
        }
    }
    else
    {
        Status = CollatorNoError;
    }

    if (Status == CollatorNoError)
    {
        if (BytesToRead - FramePayloadRemaining == 0)
        {
            GotPartialFrameHeaderBytes = 0;
            CollatorState = SeekingFrameEnd;
        }

        RemainingElementaryData += BytesToRead;
        RemainingElementaryLength -= BytesToRead;
        FramePayloadRemaining -= BytesToRead;
    }

    //
    return Status;
}

////////////////////////////////////////////////////////////////////////////
///
/// Handle a block of data that is not frame aligned.
///
/// There may be the end of a frame, whole frames, the start of a frame or even just
/// the middle of the frame.
///
/// If we have incomplete blocks we build up a complete one in the saved data,
/// in order to process we need to acquire a frame plus the next header (for sync check)
/// we can end up with the save buffer having a frame + part of a header, and a secondary
/// save with just part of a header.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudio_c::HandleElementaryStream(unsigned int Length, unsigned char *Data)
{
    CollatorStatus_t Status;

    //

    if (DiscardPesPacket)
    {
        SE_DEBUG(group_collator_audio, "Discarding %d bytes of elementary stream\n", Length);
        return CodecNoError;
    }

    //
    // Copy our arguments to class members so the utility functions can
    // act upon it.
    //
    RemainingElementaryOrigin = Data;
    RemainingElementaryData   = Data;
    RemainingElementaryLength = Length;
    //
    // Continually handle units of input until all input is exhausted (or an error occurs).
    //
    // Be aware that our helper functions may, during their execution, cause state changes
    // that result in a different branch being taken next time round the loop.
    //
    Status = CollatorNoError;

    while (Status == CollatorNoError &&  RemainingElementaryLength != 0)
    {
        SE_DEBUG(group_collator_audio, "ES loop has %d bytes remaining\n", RemainingElementaryLength);
        //report_dump_hex( severity_debug, RemainingElementaryData, min(RemainingElementaryLength, 188), 32, 0);

        switch (CollatorState)
        {
        case SeekingSyncWord:
            //
            // Try to lock to incoming frame headers
            //
            Status = SearchForSyncWord();
            break;

        case GotSynchronized:
        case SeekingFrameEnd:
            //
            // Read in the remains of the frame header
            //
            Status = ReadPartialFrameHeader();
            break;

        case ReadSubFrame:
        case SkipSubFrame:
            //
            // Squirrel away the frame
            //
            Status = ReadFrame();
            break;

        case GotCompleteFrame:
            //
            // Pass the accumulated subframes to the frame parser
            //
            InternalFrameFlush();
            CollatorState = ReadSubFrame;
            break;

        case ValidateFrame:
            //
            // Validate the accumulated frame for internal sync words
            //
            Status = ValidateCollatedFrame();
            break;

        default:
            // should not occur...
            SE_DEBUG(group_collator_audio, "General failure; wrong collator state");
            Status = CollatorError;
            break;
        }
    }

    if (Status != CollatorNoError)
    {
        // if anything when wrong then we need to resynchronize
        SE_DEBUG(group_collator_audio, "General failure; seeking new synchronization sequence\n");
        DiscardAccumulatedData();
        CollatorState = SeekingSyncWord;
    }

    return Status;
}


////////////////////////////////////////////////////////////////////////////
///
/// Scan the input until an appropriate PES start code is found.
///
/// Scans any Collator_Pes_c::RemainingData searching for a PES start code.
/// The configuration for this comes from Collator_Base_c::Configuration and
/// is this means that only the interesting (e.g. PES audio packets) start
/// codes will be detected.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudio_c::SearchForPesHeader(void)
{
    CollatorStatus_t Status;
    unsigned int CodeOffset;

//

    //
    // If there are any trailing start codes handle those.
    //

    while (TrailingStartCodeBytes && RemainingLength)
    {
        if (TrailingStartCodeBytes == 3)
        {
            // We've got the 0, 0, 1 so if the code is *not* in the ignore range then we've got one
            unsigned char SpecificCode = RemainingData[0];

            if (!IsCodeTobeIgnored(SpecificCode))
            {
                SE_DEBUG(group_collator_audio, "Found a trailing startcode 00, 00, 01, %x\n", SpecificCode);
                // Consume the specific start code
                RemainingData++;
                RemainingLength--;
                // Switch state (and reflect the data we are about to accumulate)
                SeekingPesHeader = false;
                GotPartialPesHeader = true;
                //assert( AccumulatedDataSize == 0 );
                GotPartialPesHeaderBytes = 4;
                // There are now no trailing start code bytes
                TrailingStartCodeBytes = 0;
                // Finally, accumulate the data (by reconstructing it)
                unsigned char StartCode[4] = { 0, 0, 1, SpecificCode };
                Status = AccumulateData(4, StartCode);

                if (Status != CollatorNoError)
                {
                    SE_DEBUG(group_collator_audio, "Cannot accumulate data #5 (%d)\n", Status);
                }

                return Status;
            }

            // Nope, that's not a suitable start code.
            SE_DEBUG(group_collator_audio, "Trailing start code 00, 00, 01, %x was in the ignore range\n", SpecificCode);
            TrailingStartCodeBytes = 0;
            break;
        }
        else if (TrailingStartCodeBytes == 2)
        {
            // Got two zeros, a one gets us ready to read the code.
            if (RemainingData[0] == 1)
            {
                SE_DEBUG(group_collator_audio, "Trailing start code looks good (found 00, 00; got 01)\n");
                TrailingStartCodeBytes++;
                RemainingData++;
                RemainingLength--;
                continue;
            }

            // Got two zeros, another zero still leaves us with two zeros.
            if (RemainingData[0] == 0)
            {
                SE_DEBUG(group_collator_audio, "Trailing start code looks OK (found 00, 00; got 00)\n");
                RemainingData++;
                RemainingLength--;
                continue;
            }

            // Nope, that's not a suitable start code.
            SE_DEBUG(group_collator_audio, "Trailing 00, 00 was not part of a start code\n");
            TrailingStartCodeBytes = 0;
            break;
        }
        else if (TrailingStartCodeBytes == 1)
        {
            // Got one zero, another zero gives us two (duh).
            if (RemainingData[0] == 0)
            {
                SE_DEBUG(group_collator_audio, "Trailing start code looks good (found 00; got 00)\n");
                TrailingStartCodeBytes++;
                RemainingData++;
                RemainingLength--;
                continue;
            }

            // Nope, that's not a suitable start code.
            SE_DEBUG(group_collator_audio, "Trailing 00 was not part of a start code\n");
            TrailingStartCodeBytes = 0;
            break;
        }
        else
        {
            SE_ERROR("TrailingStartCodeBytes has illegal value: %d\n", TrailingStartCodeBytes);
            TrailingStartCodeBytes = 0;
            return CollatorError;
        }
    }

    if (RemainingLength == 0)
    {
        return CollatorNoError;
    }

    //assert(TrailingStartCodeBytes == 0);
//
    Status = FindNextStartCode(&CodeOffset);

    if (Status == CollatorNoError)
    {
        SE_DEBUG(group_collator_audio, "Locked to PES packet boundaries (offset=%d)\n", CodeOffset);
        // discard any data leading up to the start code
        RemainingData += CodeOffset;
        RemainingLength -= CodeOffset;
        // switch state
        SeekingPesHeader = false;
        GotPartialPesHeader = true;
        GotPartialPesHeaderBytes = 0;
    }
    else
    {
        // examine the end of the buffer to determine if there is a (potential) trailing start code
        //assert( RemainingLength >= 1 );
        if (RemainingData[RemainingLength - 1] <= 1)
        {
            unsigned char LastBytes[3];
            LastBytes[0] = (RemainingLength >= 3 ? RemainingData[RemainingLength - 3] : 0xff);
            LastBytes[1] = (RemainingLength >= 2 ? RemainingData[RemainingLength - 2] : 0xff);
            LastBytes[2] = RemainingData[RemainingLength - 1];

            if (LastBytes[0] == 0 && LastBytes[1] == 0 && LastBytes[2] == 1)
            {
                TrailingStartCodeBytes = 3;
            }
            else if (LastBytes[1] == 0 && LastBytes[2] == 0)
            {
                TrailingStartCodeBytes = 2;
            }
            else if (LastBytes[2] == 0)
            {
                TrailingStartCodeBytes = 1;
            }
        }

        SE_DEBUG(group_collator_audio, "Discarded %d bytes while searching for PES header (%d might be start code)\n",
                 RemainingLength, TrailingStartCodeBytes);
        RemainingLength = 0;
    }

//
    return CollatorNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Scan the input until an appropriate PES start code is found.
///
/// Scan the buffer given in parameter until an appropriate
/// ControlData start code is found (0x00 0x00 0x01 0xFB)
///
/// \return true if a start code is found, in which case the
/// position of the start code is returned in the offset parameter
/// \return false if no start code is found, in which case the number
/// of trailing start code bytes is returned in the trailingStartCodeBytes
/// parameter. trailingStartCodeBytes is in [0..3]
///
bool Collator_PesAudio_c::FindNextControlDataStartCode(unsigned char *data, int length, int *offset, int *trailingStartCodeBytes)
{
    int startCodeBytes = 0;

    *trailingStartCodeBytes = 0;

    for (int i = 0; i < length; i++)
    {
        if (startCodeBytes == 0)
        {
            if (data[i] == 0x00) { startCodeBytes++; }
        }
        else if (startCodeBytes == 1)
        {
            if (data[i] == 0x00) { startCodeBytes++; }
            else { startCodeBytes = 0; }
        }
        else if (startCodeBytes == 2)
        {
            if (data[i] == 0x01) { startCodeBytes++; }
            else if (data[i] != 0x00) { startCodeBytes = 0; }
        }
        else if (startCodeBytes == 3)
        {
            if (data[i] == 0xFB)
            {
                *offset = i - 3;
                return true;
            }
            else if (data[i] == 0x00) { startCodeBytes = 1; }
            else { startCodeBytes = 0; }
        }
    }

    *trailingStartCodeBytes = startCodeBytes;

    return false;
}

void Collator_PesAudio_c::SearchForControlData(void)
{
    int CodeOffset, TrailingStartCodeBytes;

    bool ControlDataStartCodeFound = FindNextControlDataStartCode(mControlDataRemainingData, mControlDataRemainingLength, &CodeOffset, &TrailingStartCodeBytes);

    if (ControlDataStartCodeFound)
    {
        SE_DEBUG(group_collator_audio, "Found marker frame start code at offset %d!\n", CodeOffset);

        // discard any data leading up to the start code
        mControlDataRemainingData        += CodeOffset;
        mControlDataRemainingLength      -= CodeOffset;

        // switch state
        mControlDataDetectionState       = GotPartialControlData;
        mGotPartialControlDataBytes      = 0;
    }
    else
    {
        SE_DEBUG(group_collator_audio, "Discarded %d bytes while searching for PES header\n", mControlDataRemainingLength - TrailingStartCodeBytes);

        mControlDataRemainingData        += (mControlDataRemainingLength - TrailingStartCodeBytes);

        // Set the remaining length to 0 as if there are some trailing start code bytes
        // they will be consumed by accumulating them in our mStoredControlData below
        mControlDataRemainingLength      = 0;

        switch (TrailingStartCodeBytes)
        {
        case 3:
            mStoredControlData[2] = 0x01; // FALLTHROUGH
        case 2:
            mStoredControlData[1] = 0x00; // FALLTHROUGH
        case 1:
            mStoredControlData[0] = 0x00;
            SE_DEBUG(group_collator_audio, "%d trailing start code bytes => Changing state to GotPartialControlData\n", TrailingStartCodeBytes);
            mControlDataDetectionState = GotPartialControlData;
            mGotPartialControlDataBytes = TrailingStartCodeBytes;
            break;
        case 0:
            break;
        default:
            SE_ASSERT(0);
            break;
        }
    }
}

////////////////////////////////////////////////////////////////////////////
///
/// Accumulate a marker frame
///
/// Accumulate incoming data until we have the size of a marker frame
/// Then check that it is indeed a marker frame
///
/// \return Collator status code, CollatorNoError indicates success.
///
void Collator_PesAudio_c::ReadPartialControlData(void)
{
    if (mGotPartialControlDataBytes < PES_CONTROL_SIZE)
    {
        SE_DEBUG(group_collator_audio, "Accumulating ControlData Frame\n");

        unsigned int BytesToRead = min(mControlDataRemainingLength, PES_CONTROL_SIZE - mGotPartialControlDataBytes);
        memcpy(mStoredControlData + mGotPartialControlDataBytes, mControlDataRemainingData, BytesToRead);

        mGotPartialControlDataBytes += BytesToRead;
        mControlDataRemainingData += BytesToRead;
        mControlDataRemainingLength -= BytesToRead;

        return;
    }

    //
    // We now have accumulated the size of a marker frame. Let's check if it is one!
    //

    // Check start code. This is needed as in case of trailing start code bytes,
    // we may have an invalid start code
    if (mStoredControlData[0] != 0x00 || mStoredControlData[1] != 0x00 || mStoredControlData[2] != 0x01 || mStoredControlData[3] != 0xFB) { goto invalid_marker_frame; }

    // Check PES_packet_length
    if (mStoredControlData[4] != 0x00 || mStoredControlData[5] != 0x14) { goto invalid_marker_frame; }

    // Check flags
    if (mStoredControlData[6] != 0x80 || mStoredControlData[7] != 0x01) { goto invalid_marker_frame; }

    // Check PES_header_data_length
    if (mStoredControlData[8] != 0x11) { goto invalid_marker_frame; }

    // Check extension flags
    if (mStoredControlData[9] != 0x80) { goto invalid_marker_frame; }

    // Check ascii marker pattern
    if (mStoredControlData[10] != 'S' || mStoredControlData[11] != 'T' || mStoredControlData[12] != 'M' || mStoredControlData[13] != 'M') { goto invalid_marker_frame; }

    SE_DEBUG(group_collator_audio, "Found a marker frame!\n");

    mControlDataDetectionState = GotControlData;

    return;

invalid_marker_frame:
    SE_DEBUG(group_collator_audio, "Not a marker frame!\n");

    mControlDataDetectionState = SeekingControlData;
}

////////////////////////////////////////////////////////////////////////////
///
/// Accumulate incoming data until we have the full PES header.
///
/// Strictly speaking this method handles two sub-states. In the first state
/// we do not have sufficient data accumulated to determine how long the PES
/// header is. In the second we still don't have a complete PES packet but
/// at least we know how much more data we need.
///
/// This code assumes that PES packet uses >=9 bytes PES headers rather than
/// the 6 byte headers found in the program stream map, padding stream,
/// private stream 2, etc.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudio_c::ReadPartialPesHeader(void)
{
    CollatorStatus_t Status;
    unsigned int PesHeaderBytes, BytesNeeded, BytesToRead;
    unsigned char PesPrivateData[MAX_PES_PRIVATE_DATA_LENGTH];

//

    if (GotPartialPesHeaderBytes < PES_INITIAL_HEADER_SIZE)
    {
        SE_DEBUG(group_collator_audio, "Waiting for first part of PES header\n");
        StoredPesHeader = BufferBase + AccumulatedDataSize - GotPartialPesHeaderBytes;
        BytesToRead = min(RemainingLength, PES_INITIAL_HEADER_SIZE - GotPartialPesHeaderBytes);
        Status = AccumulateData(BytesToRead, RemainingData);

        if (Status == CollatorNoError)
        {
            GotPartialPesHeaderBytes += BytesToRead;
            RemainingData += BytesToRead;
            RemainingLength -= BytesToRead;
        }
        else
        {
            SE_DEBUG(group_collator_audio, "Cannot accumulate data #6 (%d)\n", Status);
        }

        return Status;
    }

    //
    // We now have accumulated sufficient data to know how long the PES header actually is!
    //
    // pass the stream_id field to the collator sub-class (might update Configuration.ExtendedHeaderLength)
    SetPesPrivateDataLength(StoredPesHeader[3]);
    PesHeaderBytes = PES_INITIAL_HEADER_SIZE + StoredPesHeader[8] + Configuration.ExtendedHeaderLength;
    BytesNeeded = PesHeaderBytes - GotPartialPesHeaderBytes;
    BytesToRead = min(RemainingLength, BytesNeeded);
    Status = AccumulateData(BytesToRead, RemainingData);

    if (Status == CollatorNoError)
    {
        GotPartialPesHeaderBytes += BytesToRead;
        RemainingData += BytesToRead;
        RemainingLength -= BytesToRead;
        SE_DEBUG(group_collator_audio, "BytesNeeded %d; BytesToRead %d\n", BytesNeeded, BytesToRead);

        if (BytesNeeded == BytesToRead)
        {
            //
            // We've got the whole header, examine it and change state
            //
            SE_DEBUG(group_collator_audio, "Got entire PES header\n");
            //report_dump_hex( severity_debug, StoredPesHeader, PesHeaderBytes, 32, 0);
            Status = CollatorNoError; // strictly speaking this is a no-op but the code might change

            if (StoredPesHeader[0] != 0x00 || StoredPesHeader[1] != 0x00 || StoredPesHeader[2] != 0x01 ||
                CollatorNoError != (Status = ReadPesHeader()))
            {
                SE_DEBUG(group_collator_audio, "%s; seeking new PES header",
                         (Status == CollatorNoError ? "Start code not where expected" :
                          "Badly formed PES header"));
                SeekingPesHeader = true;
                DiscardAccumulatedData();
                Stream->Statistics().CollatorAudioPesSyncLostCount++;
                // we have contained the error by changing states...
                return CollatorNoError;
            }

            //
            // Placeholder: Generic stream id based PES filtering (configured by sub-class) could be inserted
            //              here (set DiscardPesPacket to true to discard).
            //

            if (Configuration.ExtendedHeaderLength)
            {
                // store a pointer to the PES private header. it is located just above the end of the
                // accumulated data and is will be safely accumulated providing the private header is
                // smaller than the rest of the PES packet. if a very large PES private header is
                // encountered we will need to introduce a temporary buffer to store the header in.
                if (Configuration.ExtendedHeaderLength <= MAX_PES_PRIVATE_DATA_LENGTH)
                {
                    memcpy(PesPrivateData, BufferBase + AccumulatedDataSize - Configuration.ExtendedHeaderLength, Configuration.ExtendedHeaderLength);
                }
                else
                {
                    SE_ERROR("Implementation error: Pes Private data area too big for temporay buffer\n");
                }

                Status = HandlePesPrivateData(PesPrivateData);

                if (Status != CollatorNoError)
                {
                    SE_ERROR("Unhandled error when parsing PES private data\n");
                    return (Status);
                }
            }

            // discard the actual PES packet from the accumulate buffer
            if (AccumulatedDataSize >= PesHeaderBytes)
            {
                AccumulatedDataSize -= PesHeaderBytes;
            }
            else
            {
                SE_ERROR("Implementation Error AccumulatedDataSize Less than the PesHeaderBytes  : AccumulatedDataSize = %d PesHeaderBytes = %d\n", AccumulatedDataSize, PesHeaderBytes);
                SeekingPesHeader = true;
                DiscardAccumulatedData();
                Stream->Statistics().CollatorAudioPesSyncLostCount++;
                // we have contained the error by changing states...
                return CollatorNoError;
            }

            // record the number of bytes we need to ignore before we reach the next start code
            PesPayloadRemaining = PesPayloadLength;
            // switch states and absorb the packet
            SE_DEBUG(group_collator_audio, "Discovered PES packet (header %d bytes, payload %d bytes)\n",
                     PesPacketLength - PesPayloadLength + 6, PesPayloadLength);
            GotPartialPesHeader = false;

            if (PassPesPrivateDataToElementaryStreamHandler && Configuration.ExtendedHeaderLength)
            {
                // update PesPacketLength (to ensure that GetOffsetIntoPacket gives the correct value)
                PesPayloadLength += Configuration.ExtendedHeaderLength;
                Status = HandleElementaryStream(Configuration.ExtendedHeaderLength, PesPrivateData);

                if (Status != CollatorNoError)
                {
                    SE_ERROR("Failed not accumulate the PES private data area\n");
                }
            }
        }
    }
    else
    {
        SE_DEBUG(group_collator_audio, "Cannot accumulate data #7 (%d)\n", Status);
    }

    return Status;
}

////////////////////////////////////////////////////////////////////////////
///
/// Pass data to the elementary stream handler until the PES payload is consumed.
///
/// When all data has been consumed then switch back to the GotPartialPesHeader
/// state since (if the stream is correctly formed) the next three bytes will
/// contain the PES start code.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudio_c::ReadPesPacket(void)
{
    unsigned int BytesToRead = min(PesPayloadRemaining, RemainingLength);
    CollatorStatus_t Status = HandleElementaryStream(BytesToRead, RemainingData);
    if (Status == CollatorNoError)
    {
        if (BytesToRead == PesPayloadRemaining)
        {
            GotPartialPesHeader = true;
            GotPartialPesHeaderBytes = 0;
        }

        RemainingData += BytesToRead;
        RemainingLength -= BytesToRead;
        PesPayloadRemaining -= BytesToRead;
    }

    return CollatorNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Reset the elementary stream state machine.
///
/// If we are forced to emit a frame the elementary stream state machine (and any
/// control variables) must be reset.
///
/// This method exists primarily for sub-classes to hook. For most sub-classes
/// the trivial reset logic below is sufficient but those that maintain state
/// (especially those handling PES private data) will need to work harder.
///
void Collator_PesAudio_c::ResetCollatorStateAfterForcedFrameFlush()
{
    // we have been forced to emit the frame against our will (e.g. someone other that the collator has
    // caused the data to be emitted). we therefore have to start looking for a new sync word. don't worry
    // if the external geezer got it right this will be right in front of our nose.
    CollatorState = SeekingSyncWord;
}

CollatorStatus_t   Collator_PesAudio_c::HandleWronglyAccumulatedControlData(
    PlayerInputDescriptor_t *Input, bool NonBlocking, unsigned int *DataLengthRemaining)
{
    CollatorStatus_t Status;

    // there could be a marker frame in the accumulated data
    // start searching at offset 1 to avoid infinite loop in case
    // the accumulated data has a marker start code at the beginning
    int offset, trailingStartCodeBytes;
    bool ControlDataStartCodeFound = FindNextControlDataStartCode(mStoredControlData + 1, PES_CONTROL_SIZE - 1, &offset, &trailingStartCodeBytes);

    if (ControlDataStartCodeFound || trailingStartCodeBytes)
    {
        if (ControlDataStartCodeFound)
        {
            SE_DEBUG(group_collator_audio, "ControlData Start Code found in accumulated data\n");
            mGotPartialControlDataBytes = PES_CONTROL_SIZE - 1 - offset;
        }
        else
        {
            SE_DEBUG(group_collator_audio, "%d Trailing startcode bytes in accumulated data\n", trailingStartCodeBytes);
            mGotPartialControlDataBytes = trailingStartCodeBytes;
        }

        Status = InputSecondStage(Input, PES_CONTROL_SIZE - mGotPartialControlDataBytes, mStoredControlData, NonBlocking, DataLengthRemaining);
        memmove(mStoredControlData, mStoredControlData + PES_CONTROL_SIZE - mGotPartialControlDataBytes, mGotPartialControlDataBytes);
        mControlDataDetectionState = GotPartialControlData;
    }
    else
    {
        // no start code found, so pass the accumulated data to second stage collation
        // and stay in SeekingControlData state
        Status = InputSecondStage(Input, PES_CONTROL_SIZE, mStoredControlData, NonBlocking, DataLengthRemaining);
    }

    return Status;
}

CollatorStatus_t Collator_PesAudio_c::ProcessControlData()
{
    int controlDataType = mStoredControlData[14];

    SE_DEBUG(group_collator_audio, "Control Data Type: %d\n", controlDataType);

    switch (controlDataType)
    {
    case PES_CONTROL_REQ_TIME:
        // markerID0 is 4 byte long, from byte 16 to 19
        ControlUserData[1] = (((unsigned int)mStoredControlData[15]) << 24) + (((unsigned int)mStoredControlData[16]) << 16)
                             + (((unsigned int)mStoredControlData[17]) << 8) + ((unsigned int)mStoredControlData[18]);
        // markerID1 is 4 byte long, from byte 20 to 23
        ControlUserData[2] = (((unsigned int)mStoredControlData[19]) << 24) + (((unsigned int)mStoredControlData[20]) << 16)
                             + (((unsigned int)mStoredControlData[21]) << 8) + ((unsigned int)mStoredControlData[22]);
        SetAlarmParsedPts();
        return CollatorNoError;

    case PES_CONTROL_BRK_FWD:
    case PES_CONTROL_BRK_BWD:
        return InputJump(false, false, true);

    case PES_CONTROL_BRK_BWD_SMOOTH:
        return InputJump(false, true, true);

    case PES_CONTROL_BRK_SPLICING:
    {
        CollatorStatus_t Status = InputJump(false, false, true);
        if (Status != CollatorNoError) { return Status; }

        stm_marker_splicing_data_t SplicingMarkerData;

        SplicingMarkerData.splicing_flags = (unsigned int) mStoredControlData[15];
        SplicingMarkerData.PTS_offset = ((int64_t)mStoredControlData[20] << 32) + (((uint64_t) mStoredControlData[16]) << 24) + (((uint64_t)mStoredControlData[17]) << 16)
                                        + (((uint64_t)mStoredControlData[18]) << 8) + ((uint64_t)mStoredControlData[19]);

        // Sign extend from 40 bits to 64 bits
        SplicingMarkerData.PTS_offset = (SplicingMarkerData.PTS_offset << 24) >> 24;

        return InsertMarkerFrameToOutputPort(Stream, SplicingMarker, (void *) &SplicingMarkerData);
    }

    default:
        SE_ERROR("Unsupported Control Data type\n");
        return CollatorError;
    }
}

////////////////////////////////////////////////////////////////////////////
///
/// De-packetize incoming data and pass it to the elementary stream handler.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t   Collator_PesAudio_c::DoInput(PlayerInputDescriptor_t *Input,
                                                unsigned int             DataLength,
                                                void                    *Data,
                                                bool                     NonBlocking,
                                                unsigned int            *DataLengthRemaining)
{
    unsigned long long      Now = OS_GetTimeInMicroSeconds();

    st_relayfs_write_se(ST_RELAY_TYPE_PES_AUDIO_BUFFER, ST_RELAY_SOURCE_SE,
                        (unsigned char *)Data, DataLength, &Now);
    SE_ASSERT(!NonBlocking);
    AssertComponentState(ComponentRunning);

    // If we are running InputJump(), wait until it finish, then resume data injection
    if (OS_MutexIsLocked(&InputJumpLock))
    {
        SE_INFO(group_collator_audio, "InputJumpLock locked: Drain process is running! Awaiting unlock..\n");
    }

    OS_LockMutex(&InputJumpLock);
    InputEntry(Input, DataLength, Data, NonBlocking);
    ActOnInputDescriptor(Input);

    CollatorStatus_t Status = CollatorNoError;

    if (ESSupportsControlDataInsertion())
    {
        unsigned char *ChunkStart  = static_cast<unsigned char *>(Data);
        mControlDataRemainingData        = static_cast<unsigned char *>(Data);
        mControlDataRemainingLength      = DataLength;

        // Continually handle units of input until all input is exhausted (or an error occurs).
        //
        // Be aware that our helper functions may, during their execution, cause state changes
        // that result in a different branch being taken next time round the loop.
        //

        while (Status == CollatorNoError &&  mControlDataRemainingLength != 0)
        {
            // Check if not in low power state
            if (Playback->IsLowPowerState())
            {
                // Stop processing data to speed-up low power enter procedure (bug 24248)
                break;
            }

            if (mControlDataDetectionState == SeekingControlData)
            {
                SearchForControlData();

                if (ChunkStart != mControlDataRemainingData)
                {
                    // pass the data that has been consumed to second stage collation
                    Status = InputSecondStage(Input, mControlDataRemainingData - ChunkStart, ChunkStart, NonBlocking, DataLengthRemaining);
                }
            }
            else if (mControlDataDetectionState == GotPartialControlData)
            {
                ReadPartialControlData();

                if (mControlDataDetectionState == SeekingControlData)
                {
                    Status = HandleWronglyAccumulatedControlData(Input, NonBlocking, DataLengthRemaining);
                    if (mControlDataDetectionState == SeekingControlData) { ChunkStart = mControlDataRemainingData; }
                }
            }
            else if (mControlDataDetectionState == GotControlData)
            {
                ProcessControlData();

                // switch back to SeekingControlData state from position
                // right after the Control Data we just found
                mControlDataDetectionState = SeekingControlData;
                ChunkStart = mControlDataRemainingData;
            }
        }
    }
    else
    {
        Status = InputSecondStage(Input, DataLength, Data, NonBlocking, DataLengthRemaining);
    }

    InputExit();

    OS_UnLockMutex(&InputJumpLock);

    return Status;
}

CollatorStatus_t   Collator_PesAudio_c::InputSecondStage(PlayerInputDescriptor_t    *Input,
                                                         unsigned int        DataLength,
                                                         void           *Data,
                                                         bool            NonBlocking,
                                                         unsigned int       *DataLengthRemaining)
{
    CollatorStatus_t    Status;

    //
    // Copy our arguments to class members so the utility functions can
    // act upon it.
    //
    RemainingData   = (unsigned char *)Data;
    RemainingLength = DataLength;
    //
    // Continually handle units of input until all input is exhausted (or an error occurs).
    //
    // Be aware that our helper functions may, during their execution, cause state changes
    // that result in a different branch being taken next time round the loop.
    //
    Status = CollatorNoError;

    while (Status == CollatorNoError &&  RemainingLength != 0)
    {
        // Check if not in low power state
        if (Playback->IsLowPowerState())
        {
            // Stop processing data to speed-up low power enter procedure (bug 24248)
            break;
        }

        //SE_DEBUG(group_collator_audio, "De-PESing loop has %d bytes remaining\n", RemainingLength);
        //report_dump_hex( severity_debug, RemainingData, min(RemainingLength, 188), 32, 0);

        if (SeekingPesHeader)
        {
            //
            // Try to lock to incoming PES headers
            //
            Status = SearchForPesHeader();
        }
        else if (GotPartialPesHeader)
        {
            //
            // Read in the remains of the PES header
            //
            Status = ReadPartialPesHeader();
        }
        else
        {
            //
            // Send the PES packet for frame level analysis
            //
            Status = ReadPesPacket();
        }
    }

    if (Status != CollatorNoError)
    {
        // if anything when wrong then we need to resynchronize
        SE_DEBUG(group_collator_audio, "General failure; seeking new PES header\n");
        DiscardAccumulatedData();
        SeekingPesHeader = true;
    }

    return Status;
}

////////////////////////////////////////////////////////////////////////////
///
/// Pass accumulated data to the output ring and maintain class state variables.
///
/// \todo If we were more clever about buffer management we wouldn't have to
///       copy the frame header onto the stack.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t   Collator_PesAudio_c::InternalFrameFlush(void)
{
    CollatorStatus_t    Status;
    unsigned char       CopiedFrameHeader[MAX_FRAME_HEADER_LENGTH];

    AssertComponentState(ComponentRunning);

    SE_DEBUG(group_collator_audio, ">><<\n");

    // temporarily copy the following frame header (if there is one) to the stack
    if (AccumulatedFrameReady)
    {
        memcpy(CopiedFrameHeader, StoredFrameHeader, FrameHeaderLength);
        AccumulatedDataSize -= FrameHeaderLength;
        //Assert( BufferBase + AccumulatedDataLength == StoredFrameHeader );
    }

    // now pass the complete frame onward
    if ((!AccumulatedFrameReady) && (GotPartialFrameHeaderBytes != 0))  // Remove Partial header if any during drain
    {
        DiscardAccumulatedData();
    }

    Status                  = Collator_Pes_c::InternalFrameFlush();

    if (Status != CodecNoError)
    {
        return Status;
    }

    Framecount++;

    if (AccumulatedFrameReady)
    {
        // put the stored frame header into the new buffer
        Status = AccumulateData(FrameHeaderLength, CopiedFrameHeader);

        if (Status != CollatorNoError)
        {
            SE_DEBUG(group_collator_audio, "Cannot accumulate data #8 (%d)\n", Status);
        }

        AccumulatedFrameReady = false;
    }
    else
    {
        ResetCollatorStateAfterForcedFrameFlush();
    }

    return CodecNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Call the super-class DiscardAccumulatedData and make sure that AccumlatedFrameReady is false.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t   Collator_PesAudio_c::DiscardAccumulatedData(void)
{
    CollatorStatus_t Status;
    Status = Collator_Pes_c::DiscardAccumulatedData();
    AccumulatedFrameReady = false;
    GotPartialFrameHeaderBytes = 0;
    return Status;
}

////////////////////////////////////////////////////////////////////////////
///
/// Examine the PES private data header.
///
/// This is a stub implementation for collators that don't have a private
/// data area.
///
/// Collators that do must re-implement this method in order to extract any
/// useful data from it.
///
/// For collators that may, or may not, have a PES private data area this
/// method must also be responsible for determining which type of stream
/// is being played. See Collator_PesAudioEAc3_c::HandlePesPrivateData()
/// for more an example of such a collator.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudio_c::HandlePesPrivateData(unsigned char *PesPrivateData)
{
    return CollatorNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Validate the sanity of data in case of issue in frame size.
///
/// Assuming that the satrt has a sync word. we try to search a
/// new sync word after this
///
/// If we get a sync word we squirel of the farme and check for validity
/// of the next sync word found and check for any new frame.
/// We do not reenter any resync check to avoid any recursion
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t Collator_PesAudio_c::ValidateCollatedFrame(void)
{
    int CodeOffset = 0;
    SE_DEBUG(group_collator_audio, ">><<\n");

    if (AlreadyHandlingMissingNextFrameHeader)
    {
        SE_INFO(group_collator_audio, "Re-entered the error recovery handler, initiating stack unwind\n");
        return CollatorUnwindStack;
    }

    //
    // Remember the original elementary stream pointers for when we return to 'normal' processing.
    //
    unsigned char *OldRemainingElementaryOrigin = RemainingElementaryOrigin;
    unsigned char *OldRemainingElementaryData   = RemainingElementaryData;
    unsigned int OldRemainingElementaryLength   = RemainingElementaryLength;
    //
    // Take ownership of the already accumulated data
    //
    Buffer_t ReprocessingDataBuffer       = CodedFrameBuffer;
    unsigned char *ReprocessingData       = BufferBase;
    unsigned char *ReprocessingDataOrigin = BufferBase;
    unsigned int ReprocessingDataLength   = AccumulatedDataSize;
    ReprocessingDataBuffer->SetUsedDataSize(ReprocessingDataLength);
    BufferStatus_t BufStatus = ReprocessingDataBuffer->ShrinkBuffer(max(ReprocessingDataLength, 1));
    if (BufStatus != BufferNoError)
    {
        SE_ERROR("Failed to shrink the reprocessing buffer to size (%08x)\n", BufStatus);
        // not fatal - we're merely wasting memory
    }

    // At the time of writing GetNewBuffer() doesn't check for leaks. This is good because otherwise
    // we wouldn't have transfer the ownership of the ReprocessingDataBuffer by making this call.
    CollatorStatus_t Status = GetNewBuffer();
    if (Status != CollatorNoError)
    {
        SE_ERROR("Cannot get new buffer during error recovery\n");
        return CollatorError;
    }

    //
    // Remember that we are re-processing the previously accumulated elementary stream
    //
    AlreadyHandlingMissingNextFrameHeader = true;
    //
    // WARNING: From this point on we own the ReprocessingDataBuffer
    //
    // Remove the first byte from the recovery buffer (to avoid detecting again the same start code).
    //
    ReprocessingData += 1;
    ReprocessingDataLength -= 1;
    //
    // Search for a start code in the reprocessing data after the first valid start code.
    // We will not drop any data as we will deliver the data upto the found sync word.
    //
    RemainingElementaryOrigin = ReprocessingData;
    RemainingElementaryData   = ReprocessingData;
    RemainingElementaryLength = ReprocessingDataLength;
    PotentialFrameHeaderLength = 0; // ensure no (now voided) historic data is considered by sub-class
    Status = FindNextSyncWord(&CodeOffset);

    if (Status == CodecNoError)
    {
        SE_ASSERT(CodeOffset >= 0);
        SE_DEBUG(group_collator_audio, "Found start code during preparser buffer (byte %d of %d)\n", CodeOffset, ReprocessingDataLength);
        // We found a start code, Deliver data upto this in this point.
        // to to that we need to copy the rest of the data to the
    }
    else
    {
        // We didn't find a start code, so send any way the whole of the data
        // snip off the last few bytes. This
        // final fragment may contain a partial start code so we want to pass if through the
        // elementary stream handler again.
        // we send anyway if we get start code or not.
        SE_DEBUG(group_collator_audio, "Found no start code during validate recovery (processing final %d )\n",
                 ReprocessingDataLength);
        CodeOffset = ReprocessingDataLength;
    }

    // we will deliver in any case.
    AccumulatedFrameReady = true;
    SE_DEBUG(group_collator_audio, "Validate : AccumulateData  Size %d, Src: %p\n", (CodeOffset + 1) , ReprocessingDataOrigin);
    Status = AccumulateData((CodeOffset + 1),  ReprocessingDataOrigin);

    if (Status != CollatorNoError)
    {
        SE_DEBUG(group_collator_audio, "Validate : cannot accumulate data (%d)\n", Status);
        return Status;
    }

    StoredFrameHeader = BufferBase + (AccumulatedDataSize - FrameHeaderLength);
    //
    // update the coded frame parameters using the parameters calculated the
    // last time we saw a frame header.
    //
    if (false == CodedFrameParameters->PlaybackTimeValid)
    {
        CodedFrameParameters->PlaybackTimeValid = NextPlaybackTimeValid;
        CodedFrameParameters->PlaybackTime      = NextPlaybackTime;
    }
    if (false == CodedFrameParameters->DecodeTimeValid)
    {
        CodedFrameParameters->DecodeTimeValid   = NextDecodeTimeValid;
        CodedFrameParameters->DecodeTime        = NextDecodeTime;
    }
    InternalFrameFlush();
    CollatorState = GotSynchronized;
    ReprocessingData += CodeOffset;
    ReprocessingDataLength -= CodeOffset;
    unsigned int FinalBytes = min((unsigned int)CodeOffset, FrameHeaderLength - 1);
    ReprocessingDataLength += FinalBytes;
    ReprocessingData -= FinalBytes;

    //
    // Process the elementary stream
    //
    while (ReprocessingDataLength > 1)
    {
        //SE_INFO(group_collator_audio, "DataLength: %d, Data 0x%x\n",ReprocessingDataLength, ReprocessingData);
        Status = HandleElementaryStream(ReprocessingDataLength, ReprocessingData);

        if (CollatorNoError == Status)
        {
            SE_DEBUG(group_collator_audio, " Validate : Error recovery completed, returning to normal processing\n");
            // All data consumed and stored in the subsequent accumulation buffer
            break; // Success will propagate when we return Status
        }
        else if (CollatorUnwindStack == Status)
        {
            SE_DEBUG(group_collator_audio, "Validate :Stack unwound successfully, re-trying error recovery\n");

            // We found a frame header but lost lock again... let's have another go
            // Double check that we really are acting upon the reprocessing buffer
            if ((AccumulatedDataSize > ReprocessingDataLength) ||
                (ReprocessingData + ReprocessingDataLength !=
                 RemainingElementaryData + RemainingElementaryLength))
            {
                SE_ERROR("Internal failure during error recovery, returning to normal processing\n");
                break; // Failure will propagate when we return Status
            }

            // We found a frame header (or potentially a couple of valid frame)
            // but lost lock again... let's restart from the next byte from where we got to
            ReprocessingData       = RemainingElementaryData - (AccumulatedDataSize - 1);
            ReprocessingDataLength = RemainingElementaryLength + (AccumulatedDataSize - 1);
            AccumulatedDataSize    = 0; // make sure no accumulated data is carried round the loop
            RemainingElementaryOrigin = ReprocessingData;
            RemainingElementaryData   = ReprocessingData;
            RemainingElementaryLength = ReprocessingDataLength;
            continue;
        }
        else
        {
            SE_INFO(group_collator_audio, "Error handling elementary stream during error recovery\n");
            break; // Failure will propagate when we return Status
        }
    }

    //
    // Free the buffer we just consumed and restore the original elementary stream pointers
    //
    RemainingElementaryOrigin = OldRemainingElementaryOrigin;
    RemainingElementaryData   = OldRemainingElementaryData;
    RemainingElementaryLength = OldRemainingElementaryLength;
    (void) ReprocessingDataBuffer->DecrementReferenceCount(IdentifierCollator);
    AlreadyHandlingMissingNextFrameHeader = false;
    return Status;
}
