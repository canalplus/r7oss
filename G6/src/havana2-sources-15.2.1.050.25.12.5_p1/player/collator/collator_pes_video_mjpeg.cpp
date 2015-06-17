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
/// \class Collator_PesVideoMjpeg_c
///
/// Implements initialisation of collator video class for MJPEG
///

#include "mjpeg.h"
#include "collator_pes_video_mjpeg.h"

///
Collator_PesVideoMjpeg_c::Collator_PesVideoMjpeg_c(void)
    : Collator_PesVideo_c()
{
    Configuration.GenerateStartCodeList            = true;
    Configuration.MaxStartCodes                    = 256;       // Due to multiple RST, SOF and APP markers, larger start code list is needed
    Configuration.StreamIdentifierMask             = PES_START_CODE_MASK;
    Configuration.StreamIdentifierCode             = PES_START_CODE_VIDEO;
    Configuration.BlockTerminateMask               = 0xff;
    Configuration.BlockTerminateCode               = MJPEG_SOI;
    Configuration.IgnoreCodesRanges.NbEntries      = 2;
    Configuration.IgnoreCodesRanges.Table[0].Start = 0x00;
    Configuration.IgnoreCodesRanges.Table[0].End   = MJPEG_SOF_0 - 1;
    Configuration.IgnoreCodesRanges.Table[1].Start = MJPEG_RST_0;
    Configuration.IgnoreCodesRanges.Table[1].End   = MJPEG_RST_7;

    Configuration.InsertFrameTerminateCode         = false;         // Force the mme decode to terminate after a picture
    Configuration.TerminalCode                     = 0;
    Configuration.ExtendedHeaderLength             = 0;
    Configuration.DeferredTerminateFlag            = false;
    Configuration.StreamTerminateFlushesFrame      = false;         // Use an end of sequence to force a frame flush
    Configuration.StreamTerminationCode            = 0;
}

//{{{  FindNextStartCode
// /////////////////////////////////////////////////////////////////////////
//
//      Protected - Find the next start code (apart from any one at offset 0)
//      start codes are of the form "ff xx"

CollatorStatus_t   Collator_PesVideoMjpeg_c::FindNextStartCode(unsigned int             *CodeOffset)
{
    // If less than 2 bytes we do not bother
    if (RemainingLength < 2)
    {
        return CollatorError;
    }

    for (unsigned int i = 0; i < (RemainingLength - 2); i++)
        if (RemainingData[i] == 0xff)
        {
            if (IsCodeTobeIgnored(RemainingData[i + 1]) || (RemainingData[i + 1] == 0xff))
            {
                continue;
            }

            *CodeOffset         = i;
            return CollatorNoError;
        }

    return CollatorError;
}
//}}}
//{{{  Input
////////////////////////////////////////////////////////////////////////////
///
/// Extract Frame length from Pes Private Data area if present.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t   Collator_PesVideoMjpeg_c::Input(PlayerInputDescriptor_t        *Input,
                                                   unsigned int                    DataLength,
                                                   void                           *Data,
                                                   bool                              NonBlocking,
                                                   unsigned int                     *DataLengthRemaining)
{
    CollatorStatus_t    Status                  = CollatorNoError;
    unsigned char      *DataBlock               = (unsigned char *)Data;
    unsigned char      *PesHeader;
    unsigned int        PesLength;
    unsigned int        PayloadLength;
    unsigned int        Offset;
    unsigned int        CodeOffset;
    unsigned int        Code;
    AssertComponentState(ComponentRunning);
    SE_ASSERT(!NonBlocking);
    InputEntry(Input, DataLength, Data, NonBlocking);
    Offset                      = 0;
    RemainingData               = (unsigned char *)Data;
    RemainingLength             = DataLength;
    TerminationFlagIsSet        = false;
    Offset                      = 0;

    while (Offset < DataLength)
    {
        // Check if not in low power state
        if (Playback->IsLowPowerState())
        {
            // Stop processing data to speed-up low power enter procedure (bug 24248)
            break;
        }

        // Read the length of the payload
        PesHeader               = DataBlock + Offset;

        if (DataLength >= 9 &&
            (PesHeader[0] == 0) && (PesHeader[1] == 0) && (PesHeader[2] == 1) &&
            (PesHeader[3] == Configuration.StreamIdentifierCode))
        {
            PesLength               = (PesHeader[4] << 8) + PesHeader[5];

            if (PesLength != 0)
            {
                PayloadLength       = PesLength - PesHeader[8] - 3;

                if ((PesLength - PesHeader[8]) < 3)
                {
                    SE_ERROR("PES Header %02X %02X %02X %02X %02X %02X %02X %02X inconsisten with PesLength(%d)\n",
                             PesHeader[0], PesHeader[1], PesHeader[2], PesHeader[3],
                             PesHeader[4], PesHeader[5], PesHeader[6], PesHeader[7],
                             PesLength);
                    PayloadLength   = 0;

                    // skip the packet
                    Offset += PesLength + 6;        // PES packet is PesLength + 6 bytes long
                    continue;
                }
            }
            else
            {
                PayloadLength       = 0;
            }

            SE_DEBUG(group_collator_video, "DataLength %d, PesLength %d; PayloadLength %d, Offset %d\n", DataLength, PesLength, PayloadLength, Offset);
            Offset                 += PesLength + 6;        // PES packet is PesLength + 6 bytes long
            Bits.SetPointer(PesHeader + 9);                 // Set bits pointer ready to process optional fields

            if ((PesHeader[7] & 0x80) == 0x80)              // PTS present?
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
                SE_DEBUG(group_collator_video, "PTS %llu\n", PlaybackTime);
            }

            //}}}
            if ((PesHeader[7] & 0xC0) == 0xC0)              // DTS present?
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
                DiscardAccumulatedData();                   // throw away previous frame as incomplete
                InputExit();
                return CollatorError;
            }

            RemainingData           = PesHeader + (PesLength + 6 - PayloadLength);
            RemainingLength         = PayloadLength;

            if ((RemainingData + RemainingLength) > (DataBlock + DataLength))
            {
                RemainingLength     = (DataBlock + DataLength) - RemainingData;
            }
        }
        else
        {
            RemainingData           = DataBlock + Offset;
            RemainingLength         = DataLength -  Offset;
            Offset                  = DataLength;
        }

        while (RemainingLength > 0)
        {
            Status              = FindNextStartCode(&CodeOffset);

            if (Status != CollatorNoError)              // Error indicates no start code found
            {
                Status          = AccumulateData(RemainingLength, RemainingData);

                if (Status != CollatorNoError)
                {
                    DiscardAccumulatedData();
                }

                RemainingLength = 0;
                break;
            }

            // Got one accumulate up to and including it
            Status              = AccumulateData(CodeOffset + 2, RemainingData);

            if (Status != CollatorNoError)
            {
                DiscardAccumulatedData();
                break;
            }

            Code                        = RemainingData[CodeOffset + 1];
            RemainingLength            -= CodeOffset + 2;
            RemainingData              += CodeOffset + 2;

            // Is it a block terminate code
            if ((Code == Configuration.BlockTerminateCode) && (AccumulatedDataSize > 2))
            {
                AccumulatedDataSize    -= 2;
                Status          = InternalFrameFlush((Configuration.StreamTerminateFlushesFrame && (Code == Configuration.StreamTerminationCode)));

                if (Status != CollatorNoError)
                {
                    break;
                }

                BufferBase[0]           = 0xff;
                BufferBase[1]           = Code;
                AccumulatedDataSize     = 2;
            }

            // Accumulate the start code
            Status                      = AccumulateStartCode(PackStartCode(AccumulatedDataSize - 2, Code));

            if (Status != CollatorNoError)
            {
                DiscardAccumulatedData();
                InputExit();
                return Status;
            }
        }
    }

    InputExit();
    return CollatorNoError;
}
//}}}

