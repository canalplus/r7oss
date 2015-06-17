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

#include "collator_pes_video_raw.h"

#define CodeToInteger(a,b,c,d)          ((a << 0) | (b << 8) | (c << 16) | (d << 24))

//
Collator_PesVideoRaw_c::Collator_PesVideoRaw_c(void)
    : Collator_PesVideo_c()
    , DataRemaining(0)
    , DataCopied(0)
    , DecodeBuffer(NULL)
    , StreamInfo()
    , LastGetDecodeBufferFormat(FormatUnknown)
{
    Configuration.GenerateStartCodeList            = false;   // Packets have no start codes
    Configuration.MaxStartCodes                    = 0;
    Configuration.StreamIdentifierMask             = 0x00;
    Configuration.StreamIdentifierCode             = 0x00;
    Configuration.BlockTerminateMask               = 0x00;
    Configuration.BlockTerminateCode               = 0x00;
    Configuration.IgnoreCodesRanges.NbEntries      = 0;
    Configuration.IgnoreCodesRanges.Table[0].Start = 0x00;
    Configuration.IgnoreCodesRanges.Table[0].End   = 0x00;
    Configuration.InsertFrameTerminateCode         = false;
    Configuration.TerminalCode                     = 0x00;
    Configuration.ExtendedHeaderLength             = 0;
    Configuration.DeferredTerminateFlag            = false;
}

//{{{  Input
////////////////////////////////////////////////////////////////////////////
//
// Input, extract contrl data from the descriptor, get a buffer from the manifestor and
// copy raw data into decode buffer. Construct a dvp style info frame and pass it on
//

CollatorStatus_t   Collator_PesVideoRaw_c::Input(PlayerInputDescriptor_t *Input,
                                                 unsigned int             DataLength,
                                                 void                    *Data,
                                                 bool                     NonBlocking,
                                                 unsigned int            *DataLengthRemaining)
{
    PlayerStatus_t Status = PlayerNoError;
    SE_ASSERT(!NonBlocking);
    AssertComponentState(ComponentRunning);
    InputEntry(Input, DataLength, Data, NonBlocking);
    // Extract the descriptor timing information
    ActOnInputDescriptor(Input);
    SE_DEBUG(group_collator_video, "DataLength    : %d\n", DataLength);

    if (DataRemaining == 0)
        //{{{  Treat the packet as a header, read metadata and build stream info
    {
        DecodeBufferRequest_t           BufferRequest;
        BufferFormat_t                  BufferFormat;
        class Buffer_c                 *Buffer;
        unsigned int                    Format;
        unsigned char                  *DataBlock;
        unsigned char                  *PesHeader       = (unsigned char *)Data;
        unsigned int                    PesLength;
        unsigned int                    PesHeaderLength;
        unsigned int                    PayloadLength;
        SE_DEBUG(group_collator_video, "Header : %d\n", DataLength);
        //{{{  Read pes header
        // Read the length of the payload
        PesLength                       = (PesHeader[4] << 8) + PesHeader[5];
        PesHeaderLength                 = PesHeader[8];

        if (PesLength != 0)
        {
            PayloadLength               = PesLength - PesHeaderLength - 3;
        }
        else
        {
            PayloadLength               = 0;
        }

        SE_DEBUG(group_collator_video, "DataLength %d, PesLength %d, PesHeaderLength %d, PayloadLength %d\n", DataLength, PesLength, PesHeaderLength, PayloadLength);
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
            InputExit();
            return CollatorError;
        }

        //}}}
        DataBlock                               = PesHeader + 9 + PesHeaderLength;
        //{{{  Trace
#if 0
        SE_INFO(group_collator_video, "(%d)\n%06d: ", PayloadLength, 0);

        for (int i = 0; i < PayloadLength; i++)
        {
            SE_INFO(group_collator_video, "%02x ", DataBlock[i]);

            if (((i + 1) & 0x1f) == 0)
            {
                SE_INFO(group_collator_video, "\n%06d: ", i + 1);
            }
        }

#endif

        //}}}
        // Check that this is what we think it is
        if (strcmp((char *)DataBlock, "STMicroelectronics") != 0)
        {
            //SE_INFO(group_collator_video, "Id            : %s\n", Id);
            InputExit();
            return FrameParserNoError;
        }

        DataBlock                              += strlen((char *)DataBlock) + 1;
        // Fill in stream info for frame parser
        memcpy(&StreamInfo.width, DataBlock, sizeof(unsigned int));
        DataBlock                              += sizeof(unsigned int);
        memcpy(&StreamInfo.height, DataBlock, sizeof(unsigned int));
        DataBlock                              += sizeof(unsigned int);
        memcpy(&StreamInfo.FrameRateNumerator, DataBlock, sizeof(unsigned long long));
        DataBlock                              += sizeof(unsigned long long);
        memcpy(&StreamInfo.FrameRateDenominator, DataBlock, sizeof(unsigned long long));
        DataBlock                              += sizeof(unsigned long long);
        memcpy(&Format, DataBlock, sizeof(unsigned int));
        DataBlock                              += sizeof(unsigned int);
        //memcpy (&StreamInfo.interlaced, DataBlock, sizeof (unsigned int));
        //DataBlock                              += sizeof (unsigned int);
        memcpy(&DataRemaining, DataBlock, sizeof(unsigned int));
        DataBlock                              += sizeof(unsigned int);
        StreamInfo.interlaced                   = 0;
        StreamInfo.pixel_aspect_ratio.Numerator = 1;
        StreamInfo.pixel_aspect_ratio.Denominator = 1;
        //StreamInfo.FrameRateNumerator           = 25;
        //StreamInfo.FrameRateDenominator         = 1;

        if (DecodeBuffer == NULL)
        {
            //{{{  determine buffer format
            switch (Format)
            {
            case CodeToInteger('N', 'V', '2', '2'):
                BufferFormat          = FormatVideo422_Raster;
                break;

            case CodeToInteger('R', 'G', 'B', 'P'):
                BufferFormat          = FormatVideo565_RGB;
                break;

            case CodeToInteger('R', 'G', 'B', '3'):
                BufferFormat          = FormatVideo888_RGB;
                break;

            case CodeToInteger('R', 'G', 'B', '4'):
                BufferFormat          = FormatVideo8888_ARGB;
                break;

            case CodeToInteger('Y', 'U', 'Y', 'V'):
                BufferFormat          = FormatVideo422_YUYV;
                break;

            default:
                SE_ERROR("Unsupported decode buffer format request %.4s\n", (char *) &Format);
                InputExit();
                return CollatorError;
            }

            //}}}

            //
            // If we are not already described as having this buffer type, then we switch over to it
            //

            if (BufferFormat != LastGetDecodeBufferFormat)
            {
                if (LastGetDecodeBufferFormat != FormatUnknown)
                {
                    Stream->GetDecodeBufferManager()->EnterStreamSwitch();
                }

                Status  = Stream->GetDecodeBufferManager()->InitializeSimpleComponentList(BufferFormat);

                if (Status != DecodeBufferManagerNoError)
                {
                    SE_ERROR("Unable to set buffer descriptor\n");
                    InputExit();
                    return CollatorError;
                }

                LastGetDecodeBufferFormat = BufferFormat;
            }

            // Fill out the buffer structure request
            memset(&BufferRequest, 0, sizeof(DecodeBufferRequest_t));
            BufferRequest.DimensionCount      = 2;
            BufferRequest.Dimension[0]        = StreamInfo.width;
            BufferRequest.Dimension[1]        = StreamInfo.height;
            // Ask the decode buffer manager for the buffer
            Status                            = Stream->GetDecodeBufferManager()->GetDecodeBuffer(&BufferRequest, &Buffer);

            if (Status != DecodeBufferManagerNoError)
            {
                SE_ERROR("Failed to get decode buffer\n");
                InputExit();
                return CollatorError;
            }

            StreamInfo.buffer_class             = (unsigned int *)Buffer;
            StreamInfo.buffer                   = (unsigned int *)Stream->GetDecodeBufferManager()->ComponentBaseAddress(Buffer, PrimaryManifestationComponent);
            DecodeBuffer                        = (unsigned int *)Stream->GetDecodeBufferManager()->ComponentBaseAddress(Buffer, PrimaryManifestationComponent, CachedAddress);
            CodedFrameBuffer->AttachBuffer(Buffer);                         // Attach to decode buffer (so it will be freed at the same time)
            Buffer->DecrementReferenceCount();                              // and release ownership of the buffer to the decode buffer
        }

        SE_DEBUG(group_collator_video, "ImageSize          %6d\n", DataRemaining);
        SE_DEBUG(group_collator_video, "ImageWidth         %6d\n", StreamInfo.width);
        SE_DEBUG(group_collator_video, "ImageHeight        %6d\n", StreamInfo.height);
        SE_DEBUG(group_collator_video, "Format             %.4s\n", (char *) &Format);
        Status                                  = CollatorNoError;
    }
    //}}}
    else
        //{{{  Assume packet is part of data - copy to decode buffer
    {
        if (DecodeBuffer == NULL)
        {
            SE_ERROR("No decode buffer available\n");
            InputExit();
            return CodecError;
        }

        // Transfer the packet to the next coded data frame and pass on
        memcpy(DecodeBuffer + DataCopied, Data, DataLength);
        DataRemaining                          -= DataLength;
        DataCopied                             += DataLength;

        if (DataRemaining <= 0)
        {
            Status                              = AccumulateData(sizeof(StreamInfo_t), (unsigned char *)&StreamInfo);
            DataRemaining                       = 0;
            DataCopied                          = 0;
            DecodeBuffer                        = NULL;

            if (Status != CollatorNoError)
            {
                SE_ERROR("Failed to accumulate StreamInfo\n");
                InputExit();
                return Status;
            }

            Status                              = InternalFrameFlush();

            if (Status != CollatorNoError)
            {
                SE_ERROR("Failed to flush frame\n");
            }
        }
    }

    //}}}
    InputExit();
    return Status;
}
//}}}

