/************************************************************************
Copyright (C) 2007 STMicroelectronics. All Rights Reserved.

This file is part of the Player2 Library.

Player2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Player2 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Player2 Library may alternatively be licensed under a proprietary
license from ST.

Source file name : collator_pes_frame.cpp
Author :           Julian

Implementation of the pes frame aligned collator class for player 2.
This class is used by video streams which do not contain standard mpeg
2 styyle headers - e.g wmv, flv1, vp6, rmv, etc.

Date        Modification                                    Name
----        ------------                                    --------
12-Dec-08   Created                                         Julian

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


// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "collator_pes_frame.h"
#include "st_relay.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

//{{{  Constructor
////////////////////////////////////////////////////////////////////////////
//
// Initialize the class by resetting it.
//

Collator_PesFrame_c::Collator_PesFrame_c( void )
{
    if (InitializationStatus != CollatorNoError)
        return;

    Collator_PesFrame_c::Reset();
}

//}}}
//{{{  Reset
////////////////////////////////////////////////////////////////////////////
//
// Resets and configures according to the requirements of this stream content
//

CollatorStatus_t Collator_PesFrame_c::Reset( void )
{
    CollatorStatus_t Status;

    Status      = Collator_Base_c::Reset();
    if (Status != CollatorNoError)
        return Status;

    Configuration.GenerateStartCodeList         = true;
    Configuration.MaxStartCodes                 = 128;
    Configuration.StreamIdentifierMask          = 0x00;
    Configuration.StreamIdentifierCode          = 0x00;
    Configuration.BlockTerminateMask            = 0x00;
    Configuration.BlockTerminateCode            = 0x00;
    Configuration.IgnoreCodesRangeStart         = 0x00;
    Configuration.IgnoreCodesRangeEnd           = 0x00;
    Configuration.InsertFrameTerminateCode      = false;
    Configuration.TerminalCode                  = 0x00;
    Configuration.ExtendedHeaderLength          = 0;
    Configuration.DeferredTerminateFlag         = false;

    FrameSize                                   = 0;
    RemainingDataLength                         = 0;

    return CollatorNoError;
}
//}}}

//{{{  Input
////////////////////////////////////////////////////////////////////////////
///
/// Extract Frame length from Pes Private Data area if present.
///
/// \return Collator status code, CollatorNoError indicates success.
///
CollatorStatus_t   Collator_PesFrame_c::Input  (PlayerInputDescriptor_t*        Input,
                                                unsigned int                    DataLength,
                                                void*                           Data,
                                                bool                            NonBlocking,
                                                unsigned int                   *DataLengthRemaining )
{
    CollatorStatus_t    Status                  = CollatorNoError;
    bool                PrivateDataPresent;
    unsigned char*      DataBlock               = (unsigned char*)Data;
    unsigned char*      PesHeader;
    unsigned char*      PayloadStart;
    unsigned int        PayloadLength;
    unsigned int        PesLength;
    unsigned int        Offset;

    COLLATOR_ASSERT( !NonBlocking );
    AssertComponentState( "Collator_PesFrame_c::Input", ComponentRunning );
    InputEntry( Input, DataLength, Data, NonBlocking );

    Offset                      = 0;
    while (Offset < DataLength)
    {
        // Read the length of the payload
        PrivateDataPresent      = false;
        PesHeader               = DataBlock + Offset;
        PesLength               = (PesHeader[4] << 8) + PesHeader[5];
        if (PesLength != 0)
            PayloadLength       = PesLength - PesHeader[8] - 3;
        else
            PayloadLength       = 0;
        COLLATOR_DEBUG("DataLength %d, PesLength %d; PayloadLength %d, Offset %d\n", DataLength, PesLength, PayloadLength, Offset);
        Offset                 += PesLength + 6;        // PES packet is PesLength + 6 bytes long

        Bits.SetPointer (PesHeader + 9);                // Set bits pointer ready to process optional fields
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
            COLLATOR_DEBUG("PTS %llu.\n", PlaybackTime );
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
            COLLATOR_ERROR("Malformed pes header contains DTS without PTS.\n" );
            DiscardAccumulatedData();                   // throw away previous frame as incomplete
	    InputExit();
            return CollatorError;
        }

        //{{{  walk down optional bits
        if ((PesHeader[7] & 0x20) == 0x20)              // ESCR present
            Bits.FlushUnseen(48);                       // Size of ESCR

        if ((PesHeader[7] & 0x10) == 0x10)              // ES Rate present
            Bits.FlushUnseen(24);                       // Size of ES Rate

        if ((PesHeader[7] & 0x08) == 0x08)              // Trick mode control present
            Bits.FlushUnseen(8);                        // Size of Trick mode control

        if ((PesHeader[7] & 0x04) == 0x04)              // Additional copy info present
            Bits.FlushUnseen(8);                        // Size of additional copy info

        if ((PesHeader[7] & 0x02) == 0x02)              // PES CRC present
            Bits.FlushUnseen(16);                       // Size of previous packet CRC

        if ((PesHeader[7] & 0x01) == 0x01)              // PES Extension flag
        {
            PrivateDataPresent  = Bits.Get(1);
            Bits.FlushUnseen(7);                        // Size of Pes extension data
        }
        //}}}

        if (PrivateDataPresent)
        {
            if (RemainingDataLength != 0)
            {
                COLLATOR_ERROR ("%s: Warning new frame indicated but %d bytes missing\n", __FUNCTION__, RemainingDataLength);
                DiscardAccumulatedData();               // throw away previous frame as incomplete
            }

            FrameSize           = Bits.Get(8) + (Bits.Get(8) << 8) + (Bits.Get(8) << 16);
            RemainingDataLength = FrameSize;

            COLLATOR_DEBUG ("%s: PlaybackTimeValid %d, PlaybackTime %llx, FrameSize %d\n", __FUNCTION__, PlaybackTimeValid, PlaybackTime, FrameSize);
        }

        if ((int)PayloadLength > RemainingDataLength)           // Too much data - have some packets been lost?
        {
            if (RemainingDataLength != 0)
            {
                COLLATOR_ERROR ("%s: Warning packet contains more bytes than needed %d bytes missing?\n", __FUNCTION__, RemainingDataLength);
                DiscardAccumulatedData();                       // throw away previous frame as incomplete
                //RemainingDataLength     = 0;
		//InputExit();
                //return CollatorError;
            }
            RemainingDataLength = PayloadLength;        // assume new packet is stand alone frame
        }

        AccumulateStartCode (PackStartCode(AccumulatedDataSize, 0x42));

        PayloadStart            = PesHeader + (PesLength + 6 - PayloadLength);
        Status                  = AccumulateData (PayloadLength, (unsigned char *)PayloadStart);
        if (Status != CollatorNoError)
	{
	    InputExit();
            return Status;
	}

        RemainingDataLength    -= PayloadLength;
        if (RemainingDataLength <= 0)
        {
            Status              = InternalFrameFlush();         // flush out collected frame
            if (Status != CollatorNoError )
	    {
		InputExit();
                return Status;
	    }
        }

        COLLATOR_DEBUG("%s PrivateDataPresent %d, RemainingDataLength %d, PayloadLength %d\n",
                        __FUNCTION__, PrivateDataPresent, RemainingDataLength, PayloadLength);

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
CollatorStatus_t   Collator_PesFrame_c::InternalFrameFlush( bool        FlushedByStreamTerminate )
{
    CodedFrameParameters->FollowedByStreamTerminate     = FlushedByStreamTerminate;
    return InternalFrameFlush();
}

CollatorStatus_t   Collator_PesFrame_c::InternalFrameFlush(             void )
{
    CollatorStatus_t        Status;

    AssertComponentState( "Collator_PesFrame_c::InternalFrameFlush", ComponentRunning );

    CodedFrameParameters->PlaybackTimeValid = PlaybackTimeValid;
    CodedFrameParameters->PlaybackTime      = PlaybackTime;
    PlaybackTimeValid                       = false;

    CodedFrameParameters->DecodeTimeValid   = DecodeTimeValid;
    CodedFrameParameters->DecodeTime        = DecodeTime;
    DecodeTimeValid                         = false;

    Status                                      = Collator_Pes_c::InternalFrameFlush();
    if( Status != CodecNoError )
        return Status;

    SeekingPesHeader                            = true;
    GotPartialHeader                            = false;        // New style most video
    GotPartialZeroHeader                        = false;        // Old style used by divx only
    GotPartialPesHeader                         = false;
    GotPartialPaddingHeader                     = false;
    Skipping                                    = 0;

    return CodecNoError;
}
//}}}


