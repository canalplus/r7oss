/************************************************************************
COPYRIGHT (C) STMicroelectronics 2010

Source file name : collator_pes_raw.cpp
Author :           Julian

Implementation of the raw collator for video.


Date        Modification                                    Name
----        ------------                                    --------
24-Feb-10   Created                                         Julian

************************************************************************/

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "collator_pes_video_raw.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

#define CodeToInteger(a,b,c,d)          ((a << 0) | (b << 8) | (c << 16) | (d << 24))

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
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

Collator_PesVideoRaw_c::Collator_PesVideoRaw_c( void )
{
    if( InitializationStatus != CollatorNoError )
        return;

    Collator_PesVideoRaw_c::Reset();
}
//}}}
//{{{  Reset
////////////////////////////////////////////////////////////////////////////
//
// Resets and configures according to the requirements of this stream content
//

CollatorStatus_t Collator_PesVideoRaw_c::Reset( void )
{
    CollatorStatus_t    Status;

    Status                                      = Collator_PesVideo_c::Reset();
    if( Status != CollatorNoError )
        return Status;

    Configuration.GenerateStartCodeList         = false;                                // Packets have no start codes
    Configuration.MaxStartCodes                 = 0;
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

    DataRemaining                               = 0;
    DataCopied                                  = 0;
    DecodeBuffer                                = NULL;

    return CollatorNoError;
}
//}}}

//{{{  Input
////////////////////////////////////////////////////////////////////////////
//
// Input, extract contrl data from the descriptor, get a buffer from the manifestor and
// copy raw data into decode buffer. Construct a dvp style info frame and pass it on
//

CollatorStatus_t   Collator_PesVideoRaw_c::Input(       PlayerInputDescriptor_t        *Input,
                                                        unsigned int                    DataLength,
                                                        void                           *Data,
                                                        bool                            NonBlocking,
                                                        unsigned int                   *DataLengthRemaining)
{
    PlayerStatus_t                      Status = PlayerNoError;

    COLLATOR_ASSERT( !NonBlocking );
    AssertComponentState( "Collator_PesRaw_c::Input", ComponentRunning );
    InputEntry( Input, DataLength, Data, NonBlocking );

    // Extract the descriptor timing information
    ActOnInputDescriptor( Input );

    COLLATOR_DEBUG("DataLength    : %d\n", DataLength);
    if (DataRemaining == 0)
    //{{{  Treat the packet as a header, read metadata and build stream info
    {
        BufferStructure_t               BufferStructure;
        class Buffer_c*                 Buffer;
        unsigned int                    Format;
        unsigned char*                  DataBlock;
        unsigned char*                  PesHeader       = (unsigned char*)Data;
        unsigned int                    PesLength;
        unsigned int                    PesHeaderLength;
        unsigned int                    PayloadLength;

        COLLATOR_DEBUG("Header : %d\n", DataLength);

        //{{{  Read pes header
        // Read the length of the payload
        PesLength                       = (PesHeader[4] << 8) + PesHeader[5];
        PesHeaderLength                 = PesHeader[8];
        if (PesLength != 0)
            PayloadLength               = PesLength - PesHeaderLength - 3;
        else
            PayloadLength               = 0;
        COLLATOR_DEBUG("DataLength %d, PesLength %d, PesHeaderLength %d, PayloadLength %d\n", DataLength, PesLength, PesHeaderLength, PayloadLength);

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
	    InputExit();
            return CollatorError;
        }
        //}}}

        DataBlock                               = PesHeader + 9 + PesHeaderLength;
        //{{{  Trace
        #if 0
        report (severity_info, "(%d)\n%06d: ", PayloadLength, 0);
        for (int i=0; i<PayloadLength; i++)
        {
            report (severity_info, "%02x ", DataBlock[i]);
            if (((i+1)&0x1f)==0)
                report (severity_info, "\n%06d: ", i+1);
        }
        report (severity_info, "\n");
        #endif
        //}}}
        // Check that this is what we think it is
        if (strcmp ((char*)DataBlock, "STMicroelectronics") != 0)
        {
            //COLLATOR_TRACE("Id            : %s\n", Id);
	    InputExit();
            return FrameParserNoError;
        }
        DataBlock                              += strlen ((char*)DataBlock)+1;

        // Fill in stream info for frame parser
        memcpy (&StreamInfo.width, DataBlock, sizeof (unsigned int));
        DataBlock                              += sizeof (unsigned int);
        memcpy (&StreamInfo.height, DataBlock, sizeof (unsigned int));
        DataBlock                              += sizeof (unsigned int);
        memcpy (&StreamInfo.FrameRateNumerator, DataBlock, sizeof (unsigned long long));
        DataBlock                              += sizeof (unsigned long long);
        memcpy (&StreamInfo.FrameRateDenominator, DataBlock, sizeof (unsigned long long));
        DataBlock                              += sizeof (unsigned long long);
        memcpy (&Format, DataBlock, sizeof (unsigned int));
        DataBlock                              += sizeof (unsigned int);
        //memcpy (&StreamInfo.interlaced, DataBlock, sizeof (unsigned int));
        //DataBlock                              += sizeof (unsigned int);
        memcpy (&DataRemaining, DataBlock, sizeof (unsigned int));
        DataBlock                              += sizeof (unsigned int);

        StreamInfo.interlaced                   = 0;
        StreamInfo.pixel_aspect_ratio.Numerator = 1;
        StreamInfo.pixel_aspect_ratio.Denominator = 1;
        //StreamInfo.FrameRateNumerator           = 25;
        //StreamInfo.FrameRateDenominator         = 1;
        StreamInfo.InputWindow.X                = 0;
        StreamInfo.InputWindow.Y                = 0;
        StreamInfo.InputWindow.Width            = StreamInfo.width;
        StreamInfo.InputWindow.Height           = StreamInfo.height;
        StreamInfo.OutputWindow                 = StreamInfo.InputWindow;
        memset (&StreamInfo.OutputWindow, 0, sizeof(StreamInfo.OutputWindow));

        if (DecodeBuffer == NULL)
        {
            // Fill out the buffer structure request
            memset (&BufferStructure, 0x00, sizeof(BufferStructure_t));
            BufferStructure.DimensionCount      = 2;
            BufferStructure.Dimension[0]        = StreamInfo.width;
            BufferStructure.Dimension[1]        = StreamInfo.height;

            //{{{  determine buffer format
            switch (Format)
            {
                case CodeToInteger ('N','V','2','2'):
                    BufferStructure.Format          = FormatVideo422_Raster;
                    break;

                case CodeToInteger ('R','G','B','P'):
                    BufferStructure.Format          = FormatVideo565_RGB;
                    break;

                case CodeToInteger ('R','G','B','3'):
                    BufferStructure.Format          = FormatVideo888_RGB;
                    break;

                case CodeToInteger ('R','G','B','4'):
                    BufferStructure.Format          = FormatVideo8888_ARGB;
                    break;

                case CodeToInteger ('Y','U','Y','V'):
                    BufferStructure.Format          = FormatVideo422_YUYV;
                    break;

                default:
                    COLLATOR_ERROR("Unsupported decode buffer format request %.4s\n", &Format);
		    InputExit();
                    return CollatorError;
            }
            //}}}

            // Ask the manifestor for the buffer
            Status                              = Manifestor->GetDecodeBuffer (&BufferStructure, &Buffer);
            if (Status != ManifestorNoError)
            {
                COLLATOR_ERROR ("Failed to get decode buffer\n");
		InputExit();
                return CollatorError;
            }

            StreamInfo.buffer_class             = (unsigned int*)Buffer;

            // Get physical address of data buffer (not actually used later in pipeline)
            Status                              = Buffer->ObtainDataReference (NULL, NULL, (void**)&StreamInfo.buffer, PhysicalAddress);
            if (Status != BufferNoError)
            {
                COLLATOR_ERROR ("Failed to get decode buffer data reference\n");
		InputExit();
                return CollatorError;
            }

            // Remember cached address so we can write to it
            Status                              = Buffer->ObtainDataReference (NULL, NULL, (void**)&DecodeBuffer, CachedAddress);
            if (Status != BufferNoError)
            {
                COLLATOR_ERROR ("Failed to get decode buffer data reference\n");
		InputExit();
                return CollatorError;
            }

            CodedFrameBuffer->AttachBuffer (Buffer);                        // Attach to decode buffer (so it will be freed at the same time)
            Buffer->DecrementReferenceCount ();                             // and release ownership of the buffer to the decode buffer
        }

        COLLATOR_DEBUG("%s: ImageSize          %6d\n", __FUNCTION__, DataRemaining);
        COLLATOR_DEBUG("%s: ImageWidth         %6d\n", __FUNCTION__, StreamInfo.width);
        COLLATOR_DEBUG("%s: ImageHeight        %6d\n", __FUNCTION__, StreamInfo.height);
        COLLATOR_DEBUG("%s: Format           %.4s\n", __FUNCTION__, &Format);

        Status                                  = CollatorNoError;
    }
    //}}}
    else
    //{{{  Assume packet is part of data - copy to decode buffer
    {
        if (DecodeBuffer == NULL)
        {
            COLLATOR_ERROR ("No decode buffer available\n");
	    InputExit();
            return CodecError;
        }

        // Transfer the packet to the next coded data frame and pass on
        memcpy (DecodeBuffer+DataCopied, Data, DataLength);
        DataRemaining                          -= DataLength;
        DataCopied                             += DataLength;

        if (DataRemaining <= 0)
        {
            Status                              = AccumulateData (sizeof (StreamInfo_t), (unsigned char*)&StreamInfo);
            DataRemaining                       = 0;
            DataCopied                          = 0;
            DecodeBuffer                        = NULL;
            if (Status != CollatorNoError)
            {
                COLLATOR_ERROR ("Failed to accumulate StreamInfo\n");
		InputExit();
                return Status;
            }
            Status                              = InternalFrameFlush ();
            if (Status != CollatorNoError)
                COLLATOR_ERROR ("Failed to flush frame\n");
        }
    }
    //}}}

    InputExit();
    return Status;
}
//}}}

