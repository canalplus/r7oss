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
///     \class Collator_PesFrame_c
///
///     Specialized PES collator implementing for frame aligned input.
///
///     This class is used by streams without traditional mpeg2 style headers 00 00 01 x
///     or any means of determining frame boundaries.  The frame length is provided in
///     the PES private header and the collator gathers that many bytes over the
///     following PES packets.
///     Error conditions which result in frame discard are
///       Packet contains more data than remainder required
///       Packet contains private data area but still data to gather on previous frame.
///

#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"
#include "st_relayfs_se.h"
#include "collator_pes_frame.h"

// Constructor
Collator_PesFrame_c::Collator_PesFrame_c(void)
    : Collator_Pes_c()
    , RemainingDataLength(0)
    , FrameSize(0)
{
    Configuration.GenerateStartCodeList         = true;
    Configuration.MaxStartCodes                 = 128;
    Configuration.StreamIdentifierMask          = 0x00;
    Configuration.StreamIdentifierCode          = 0x00;
    Configuration.BlockTerminateMask            = 0x00;
    Configuration.BlockTerminateCode            = 0x00;
    Configuration.InsertFrameTerminateCode      = false;
    Configuration.TerminalCode                  = 0x00;
    Configuration.ExtendedHeaderLength          = 0;
    Configuration.DeferredTerminateFlag         = false;
}

//{{{  Input
////////////////////////////////////////////////////////////////////////////
///
/// Extract Frame length from Pes Private Data area if present.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t   Collator_PesFrame_c::DoInput(PlayerInputDescriptor_t        *Input,
                                                unsigned int                    DataLength,
                                                void                           *Data,
                                                bool                            NonBlocking,
                                                unsigned int                   *DataLengthRemaining)
{
    CollatorStatus_t    Status                  = CollatorNoError;
    unsigned char      *DataBlock               = (unsigned char *)Data;
    unsigned char      *DataStart;
    unsigned int        DataAvailable;
    unsigned long long  Now = OS_GetTimeInMicroSeconds();
    SE_ASSERT(Stream);

    switch (Stream->GetStreamType())
    {
    case StreamTypeAudio:
        st_relayfs_write_se(ST_RELAY_TYPE_PES_AUDIO_BUFFER, ST_RELAY_SOURCE_SE,
                            (unsigned char *)Data, DataLength, &Now);
        break;

    case StreamTypeVideo:
        st_relayfs_write_se(ST_RELAY_TYPE_PES_VIDEO_BUFFER, ST_RELAY_SOURCE_SE,
                            (unsigned char *)Data, DataLength, &Now);
        break;

    default:
        SE_ERROR("Unexpected StreamType - %s", ToString(Stream->GetStreamType()));
    }

    SE_ASSERT(!NonBlocking);
    AssertComponentState(ComponentRunning);
    InputEntry(Input, DataLength, Data, NonBlocking);

    SE_VERBOSE(GetGroupTrace(), "Searching for 00 00 01 %02x, received %02x %02x %02x %02x\n", Configuration.StreamIdentifierCode,
               DataBlock[0], DataBlock[1], DataBlock[2], DataBlock[3]);

    unsigned int Offset = 0;
    while (Offset < DataLength)
    {
        unsigned char          *PesHeader       = DataBlock + Offset;

        if ((PesHeader[0] == 0) && (PesHeader[1] == 0) && (PesHeader[2] == 1) &&
            (PesHeader[3] == Configuration.StreamIdentifierCode))
            //{{{  Pes header present
        {
            unsigned int        PesLength;
            unsigned int        PesDataLength;
            bool                PrivateDataPresent;
            // Read the length of the payload
            PrivateDataPresent          = false;
            PesLength                   = (PesHeader[4] << 8) + PesHeader[5];

            if (PesLength != 0)
            {
                PesDataLength           = PesLength - PesHeader[8] - 3;
            }
            else
            {
                PesDataLength           = 0;
            }

            //{{{  Read PTS and DTS
            Bits.SetPointer(PesHeader + 9);                     // Set bits pointer ready to process optional fields

            if ((PesHeader[7] & 0x80) == 0x80)                  // PTS present?
                //{{{  read PTS
            {
                Bits.FlushUnseen(4);
                PlaybackTime        = (unsigned long long)(Bits.Get(3)) << 30;
                Bits.FlushUnseen(1);
                PlaybackTime       |= Bits.Get(15) << 15;
                Bits.FlushUnseen(1);
                PlaybackTime       |= Bits.Get(15);
                Bits.FlushUnseen(1);
                PlaybackTimeValid   = true;
                SE_DEBUG(GetGroupTrace(), "PTS %llu\n", PlaybackTime);
            }

            //}}}
            if ((PesHeader[7] & 0xC0) == 0xC0)                  // DTS present?
                //{{{  read DTS
            {
                Bits.FlushUnseen(4);
                DecodeTime          = (unsigned long long)(Bits.Get(3)) << 30;
                Bits.FlushUnseen(1);
                DecodeTime         |= Bits.Get(15) << 15;
                Bits.FlushUnseen(1);
                DecodeTime         |= Bits.Get(15);
                Bits.FlushUnseen(1);
                DecodeTimeValid     = true;
            }
            //}}}
            else if ((PesHeader[7] & 0xC0) == 0x40)
            {
                SE_ERROR("Malformed pes header contains DTS without PTS\n");
                DiscardAccumulatedData();                       // throw away previous frame as incomplete
                InputExit();
                return CollatorError;
            }

            //}}}
            //{{{  walk down optional bits
            if ((PesHeader[7] & 0x20) == 0x20)                  // ESCR present
            {
                Bits.FlushUnseen(48);    // Size of ESCR
            }

            if ((PesHeader[7] & 0x10) == 0x10)                  // ES Rate present
            {
                Bits.FlushUnseen(24);    // Size of ES Rate
            }

            if ((PesHeader[7] & 0x08) == 0x08)                  // Trick mode control present
            {
                Bits.FlushUnseen(8);    // Size of Trick mode control
            }

            if ((PesHeader[7] & 0x04) == 0x04)                  // Additional copy info present
            {
                Bits.FlushUnseen(8);    // Size of additional copy info
            }

            if ((PesHeader[7] & 0x02) == 0x02)                  // PES CRC present
            {
                Bits.FlushUnseen(16);    // Size of previous packet CRC
            }

            if ((PesHeader[7] & 0x01) == 0x01)                  // PES Extension flag
            {
                PrivateDataPresent      = Bits.Get(1);
                Bits.FlushUnseen(7);                            // Size of Pes extension data
            }

            //}}}

            if (PrivateDataPresent)
                //{{{  Read frame size
            {
                if (RemainingDataLength != 0)
                {
                    SE_ERROR("Warning new frame indicated but %d bytes missing\n", RemainingDataLength);
                    DiscardAccumulatedData();                   // throw away previous frame as incomplete
                }

                FrameSize               = Bits.Get(8) + (Bits.Get(8) << 8) + (Bits.Get(8) << 16);
                RemainingDataLength     = FrameSize;
                SE_DEBUG(GetGroupTrace(), "PlaybackTimeValid %d, PlaybackTime %llx, FrameSize %d\n", PlaybackTimeValid, PlaybackTime, FrameSize);
            }

            //}}}
            Status = AccumulateStartCode(PackStartCode(AccumulatedDataSize, 0x42));      // Mark block boundary
            if (Status != CollatorNoError)
            {
                DiscardAccumulatedData();
                InputExit();
                return Status;
            }

            DataStart                   = PesHeader + (PesLength + 6 - PesDataLength);
            Offset                      = DataStart - DataBlock;
            DataAvailable               = DataLength - Offset;
        }
        //}}}
        else
            //{{{  No pes header grab data as payload up to RemainingDataLength
        {
            SE_DEBUG(GetGroupTrace(), "No Pes Header at %d, grabbing %d, of %d\n", Offset, DataLength, RemainingDataLength);

            if (RemainingDataLength == 0)
            {
                Offset++;                               // just ignore this byte
                DataAvailable           = 0;
            }
            else
            {
                DataAvailable           = DataLength - Offset;
                DataStart               = DataBlock + Offset;
            }
        }

        //}}}
        SE_DEBUG(GetGroupTrace(), "DataAvailable %d, RemainingDataLength %d\n", DataAvailable, RemainingDataLength);

        if (DataAvailable > 0)
            //{{{  Save available data and flush frame if complete
        {
            if ((int)DataAvailable > RemainingDataLength)
            {
                if (RemainingDataLength == 0)
                {
                    RemainingDataLength = DataAvailable;    // Assume packet is stand alone frame
                }
                else
                {
                    DataAvailable       = RemainingDataLength;    // Just take what is needed
                }
            }

            Offset                     += DataAvailable;
            Status                      = AccumulateData(DataAvailable, (unsigned char *)DataStart);

            if (Status != CollatorNoError)
            {
                InputExit();
                return Status;
            }

            RemainingDataLength        -= DataAvailable;

            if (RemainingDataLength <= 0)
            {
                Status                  = InternalFrameFlush();         // flush out collected frame

                if (Status != CollatorNoError)
                {
                    InputExit();
                    return Status;
                }
            }
        }

        //}}}
    }

    InputExit();
    return CollatorNoError;
}
//}}}
//{{{  InternalFrameFlush
// /////////////////////////////////////////////////////////////////////////
//
//      The Frame Flush functions
//
CollatorStatus_t   Collator_PesFrame_c::InternalFrameFlush(bool        FlushedByStreamTerminate)
{
    CodedFrameParameters->FollowedByStreamTerminate     = FlushedByStreamTerminate;
    return InternalFrameFlush();
}

CollatorStatus_t   Collator_PesFrame_c::InternalFrameFlush(void)
{
    AssertComponentState(ComponentRunning);

    CodedFrameParameters->PlaybackTimeValid = PlaybackTimeValid;
    CodedFrameParameters->PlaybackTime      = PlaybackTime;
    PlaybackTimeValid                       = false;
    CodedFrameParameters->DecodeTimeValid   = DecodeTimeValid;
    CodedFrameParameters->DecodeTime        = DecodeTime;
    DecodeTimeValid                         = false;

    CollatorStatus_t Status = Collator_Pes_c::InternalFrameFlush();
    if (Status != CodecNoError)
    {
        return Status;
    }

    SeekingPesHeader                            = true;
    GotPartialHeader                            = false;        // New style most video
    GotPartialZeroHeader                        = false;        // Old style used by divx only
    GotPartialPesHeader                         = false;
    GotPartialPaddingHeader                     = false;
    Skipping                                    = 0;

    return CodecNoError;
}
//}}}


