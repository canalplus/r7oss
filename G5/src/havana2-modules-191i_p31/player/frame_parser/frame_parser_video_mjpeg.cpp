/************************************************************************
Copyright (C) 2006 STMicroelectronics. All Rights Reserved.

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

Source file name : frame_parser_video_mjpeg.cpp
Author :           Julian

Implementation of the mjpeg video frame parser class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
20-01-09    Created                                         Julian

************************************************************************/

//#define DUMP_HEADERS

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "frame_parser_video_mjpeg.h"
#include "ring_generic.h"

//{{{  locally defined constants and macros
// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

static BufferDataDescriptor_t   MjpegStreamParametersBuffer       = BUFFER_MJPEG_STREAM_PARAMETERS_TYPE;
static BufferDataDescriptor_t   MjpegFrameParametersBuffer        = BUFFER_MJPEG_FRAME_PARAMETERS_TYPE;

//}}}
// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

//{{{  Constructor
// /////////////////////////////////////////////////////////////////////////
//
//      Constructor
//

FrameParser_VideoMjpeg_c::FrameParser_VideoMjpeg_c( void )
{
    Configuration.FrameParserName               = "VideoMjpeg";

    Configuration.StreamParametersCount         = 32;
    Configuration.StreamParametersDescriptor    = &MjpegStreamParametersBuffer;

    Configuration.FrameParametersCount          = 32;
    Configuration.FrameParametersDescriptor     = &MjpegFrameParametersBuffer;

    Reset();
}
//}}}
//{{{  Destructor
// /////////////////////////////////////////////////////////////////////////
//
//      Destructor
//

FrameParser_VideoMjpeg_c::~FrameParser_VideoMjpeg_c( void )
{
    Halt();
    Reset();
}
//}}}
//{{{  Reset
// /////////////////////////////////////////////////////////////////////////
//
//      The Reset function release any resources, and reset all variable
//

FrameParserStatus_t   FrameParser_VideoMjpeg_c::Reset(  void )
{
    memset( &CopyOfStreamParameters, 0x00, sizeof(MjpegStreamParameters_t) );
    memset( &ReferenceFrameList, 0x00, sizeof(ReferenceFrameList_t) );

    StreamParameters                            = NULL;
    FrameParameters                             = NULL;
    DeferredParsedFrameParameters               = NULL;
    DeferredParsedVideoParameters               = NULL;

    return FrameParser_Video_c::Reset();
}
//}}}

//{{{  RegisterOutputBufferRing
// /////////////////////////////////////////////////////////////////////////
//
//      The register output ring function
//

FrameParserStatus_t   FrameParser_VideoMjpeg_c::RegisterOutputBufferRing( Ring_t Ring )
{
    //
    // Clear our parameter pointers
    //

    StreamParameters                    = NULL;
    FrameParameters                     = NULL;
    DeferredParsedFrameParameters       = NULL;
    DeferredParsedVideoParameters       = NULL;

    //
    // Pass the call on down (we need the frame parameters count obtained by the lower level function).
    //

    return FrameParser_Video_c::RegisterOutputBufferRing( Ring );
}
//}}}
//{{{  ReadHeaders
// /////////////////////////////////////////////////////////////////////////
//
//      The read headers stream specific function
//

FrameParserStatus_t   FrameParser_VideoMjpeg_c::ReadHeaders( void )
{
    FrameParserStatus_t         Status          = FrameParserNoError;
    unsigned int                Code;
    bool                        StartOfImageSeen;

#if 0
    unsigned int                i;
    report (severity_info, "First 40 bytes of %d :", BufferLength);
    for (unsigned int i=0; i<40; i++)
        report (severity_info, " %02x", BufferData[i]);
    report (severity_info, "\n");
#endif
#if 0
    report (severity_info, "Start codes (%d):", StartCodeList->NumberOfStartCodes);
    for (unsigned int i=0; i<StartCodeList->NumberOfStartCodes; i++)
        report (severity_info, " %02x(%d)", ExtractStartCodeCode(StartCodeList->StartCodes[i]), ExtractStartCodeOffset(StartCodeList->StartCodes[i]));
    report (severity_info, "\n");
#endif

    ParsedFrameParameters->DataOffset   = 0;
    StartOfImageSeen                    = false;
    for (unsigned int i=0; i<StartCodeList->NumberOfStartCodes; i++)
    {
        Code                            = StartCodeList->StartCodes[i];
        Bits.SetPointer (BufferData + ExtractStartCodeOffset(Code));
        Bits.FlushUnseen(16);
        Status                          = FrameParserNoError;
        switch (ExtractStartCodeCode(Code))
        {
            case MJPEG_SOI:
                ParsedFrameParameters->DataOffset       = ExtractStartCodeOffset(Code);
                StartOfImageSeen                        = true;
                break;
            case  MJPEG_SOF_0:               // Read the start of frame header
            case  MJPEG_SOF_1:
                if (!StartOfImageSeen)
                {
                    FRAME_ERROR("%s - No Start Of Image seen\n", __FUNCTION__);
                    return FrameParserError;
                }

                Status                  = ReadStartOfFrame();
                if (Status == FrameParserNoError)
                    return CommitFrameForDecode();
                break;
            case MJPEG_APP_15:
                Status                  = ReadStreamMetadata();
                break;
            default:
                break;
        }
    }

    return FrameParserNoError;
}
//}}}

//{{{  ReadStreamMetadata
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Read in stream generic information
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoMjpeg_c::ReadStreamMetadata (void)
{
    MjpegVideoSequence_t*       Header;
    char                        VendorId[64];
    FrameParserStatus_t         Status          = FrameParserNoError;

    for (unsigned int i=0; i<sizeof (VendorId); i++)
    {
        VendorId[i]             = Bits.Get(8);
        if (VendorId[i] == 0)
            break;
    }
    if (strcmp (VendorId, "STMicroelectronics") != 0)
    {
        FRAME_TRACE("    VendorId          : %s\n", VendorId);
        return FrameParserNoError;
    }
    Status                      = GetNewStreamParameters ((void**)&StreamParameters);
    if (Status != FrameParserNoError)
        return Status;

    StreamParameters->UpdatedSinceLastFrame     = true;
    Header                      = &StreamParameters->SequenceHeader;
    memset (Header, 0x00, sizeof(MjpegVideoSequence_t));

    Header->time_scale          = Bits.Get(32);
    Header->time_delta          = Bits.Get(32);

    StreamParameters->SequenceHeaderPresent             = true;

#ifdef DUMP_HEADERS
    FRAME_TRACE("StreamMetadata :- \n");
    FRAME_TRACE("    VendorId          : %s\n", VendorId);
    FRAME_TRACE("    time_scale        : %6d\n", Header->time_scale);
    FRAME_TRACE("    time_delta        : %6d\n", Header->time_delta);
#endif

    return FrameParserNoError;
}
//}}}
//{{{  ReadStartOfFrame
// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read the start of frame header
//

FrameParserStatus_t   FrameParser_VideoMjpeg_c::ReadStartOfFrame( void )
{
    FrameParserStatus_t         Status;
    MjpegVideoPictureHeader_t*  Header;

    if( FrameParameters == NULL )
    {
        Status  = GetNewFrameParameters( (void **)&FrameParameters );
        if( Status != FrameParserNoError )
            return Status;
    }

    Header                                      = &FrameParameters->PictureHeader;
    memset( Header, 0x00, sizeof(MjpegVideoPictureHeader_t) );

    Header->length                              = Bits.Get(16);

    Header->sample_precision                    = Bits.Get(8);
    Header->frame_height                        = Bits.Get(16);
    Header->frame_width                         = Bits.Get(16);
    Header->number_of_components                = Bits.Get(8);

    if (Header->number_of_components >= MJPEG_MAX_COMPONENTS)
    {
        FRAME_ERROR("%s - Found more than supported number of components (%d)\n", __FUNCTION__, Header->number_of_components);
        return FrameParserError;
    }

    for (unsigned int i=0; i<Header->number_of_components; i++)
    {
        Header->components[i].id                                = Bits.Get(8);
        Header->components[i].vertical_sampling_factor          = Bits.Get(4);
        Header->components[i].horizontal_sampling_factor        = Bits.Get(4);
        Header->components[i].quantization_table_index          = Bits.Get(8);
    }

    FrameParameters->PictureHeaderPresent       = true;

#ifdef DUMP_HEADERS
    FRAME_TRACE( "Start of frame header:\n" );
    FRAME_TRACE( "        Length              %d\n", Header->length);
    FRAME_TRACE( "        Precision           %d\n", Header->sample_precision);
    FRAME_TRACE( "        FrameHeight         %d\n", Header->frame_height);
    FRAME_TRACE( "        FrameWidth          %d\n", Header->frame_width);
    FRAME_TRACE( "        NumberOfComponents  %d\n", Header->number_of_components);
    for (unsigned int i=0; i<Header->number_of_components; i++)
        FRAME_TRACE( "            Id = %d, HSF = %d, VSF = %d, QTI = %d\n",
                Header->components[i].id,
                Header->components[i].horizontal_sampling_factor,
                Header->components[i].vertical_sampling_factor,
                Header->components[i].quantization_table_index);
#endif

    return FrameParserNoError;

}
//}}}
//{{{  Unused header parsing
#if 0
//{{{  ReadQuantizationMatrices
// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read the quantization matrices.
//
FrameParserStatus_t   FrameParser_VideoMjpeg_c::ReadQuantizationMatrices( void )
{
    int                 DataSize;
    unsigned char       Temporary;
    unsigned int        Table;
    unsigned int        Precision;

    DataSize            = (int)GetShort() - 2;
    while (DataSize > 0)
    {
        Temporary       = GetByte();
        Table           = Temporary & 0xf;
        Precision       = Temporary >> 4;

        if (Table >= MAX_QUANTIZATION_MATRICES)
        {
            FRAME_TRACE("%s - Table index too large (%d)\n", __FUNCTION__, Table);
            return FrameParserError;
        }

        if (Precision != 0)
        {
            for(int i=0; i<QUANTIZATION_MATRIX_SIZE; i++)
                QuantizationMatrices[Table][MjpegCoefficientMatrixNaturalOrder[i]]      = GetShort();

            DataSize   -= 1 + (QUANTIZATION_MATRIX_SIZE*sizeof(unsigned short int));
        }
        else
        {
            for(int i=0; i<QUANTIZATION_MATRIX_SIZE; i++)
                QuantizationMatrices[Table][MjpegCoefficientMatrixNaturalOrder[i]]      = (unsigned short int)GetByte();

            DataSize   -= 1 + (QUANTIZATION_MATRIX_SIZE*sizeof(unsigned char));
        }
    }


#ifdef DUMP_HEADERS
    FRAME_TRACE( "    MJPEG_DQT  (Quantization tables)\n" );
    FRAME_TRACE( "        %04x %04x %04x %04x - %04x %04x %04x %04x - %04x %04x %04x %04x - %04x %04x %04x %04x\n",
                QuantizationMatrices[0][0], QuantizationMatrices[0][1], QuantizationMatrices[0][2], QuantizationMatrices[0][3],
                QuantizationMatrices[1][0], QuantizationMatrices[1][1], QuantizationMatrices[1][2], QuantizationMatrices[1][3], 
                QuantizationMatrices[2][0], QuantizationMatrices[2][1], QuantizationMatrices[2][2], QuantizationMatrices[2][3], 
                QuantizationMatrices[3][0], QuantizationMatrices[3][1], QuantizationMatrices[3][2], QuantizationMatrices[3][3] );
#endif

    return FrameParserNoError;

}
//}}}
//{{{  ReadRestartInterval
// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read the restart interval
//

FrameParserStatus_t   FrameParser_VideoMjpeg_c::ReadRestartInterval( void )
{
    int         DataSize;


    DataSize                                    = (int)GetShort() - 2;
    CodecStreamParameters.RestartInterval       = GetShort();

#ifdef DUMP_HEADERS
    FRAME_TRACE( "    MJPEG_DRI (Restart interval)\n" );
    FRAME_TRACE( "        %04x\n", CodecStreamParameters.RestartInterval );
#endif

    return FrameParserNoError;

}
//}}}
//{{{  ReadHuffmanTables
// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read the huffman encoding tables
//
FrameParserStatus_t   FrameParser_VideoMjpeg_c::ReadHuffmanTables( void )
{
    int                 DataSize;
    unsigned int        TableSize;

    DataSize            = (int)GetShort() - 2;

    for (int Index=0; (DataSize >= (SIZEOF_HUFFMAN_BITS_FIELD+1)); Index++)
    {
        if (Index >= MAX_SUPPORTED_HUFFMAN_TABLES)
        {
            FRAME_ERROR("%s - Found more than supported number of Huffman tables (%d)\n", __FUNCTION__, Index );
            BufferPointer      += DataSize;
            return FrameParserError;
        }

        HuffmanTables[Index].Id         = GetByte();
        ReadArray( HuffmanTables[Index].BitsTable, SIZEOF_HUFFMAN_BITS_FIELD );
        DataSize                       -= SIZEOF_HUFFMAN_BITS_FIELD;

        TableSize                       = 0;
        for (int i=0; i<SIZEOF_HUFFMAN_BITS_FIELD; i++)
            TableSize                  += HuffmanTables[Index].BitsTable[i];

        HuffmanTables[Index].DataSize   = TableSize;
        ReadArray( HuffmanTables[Index].DataTable, TableSize );
        DataSize                       -= TableSize;
    }

#ifdef DUMP_HEADERS
        FRAME_TRACE( "    MJPEG_DHT  (Huffman tables)\n" );
        for (int i=0; i<MAX_SUPPORTED_HUFFMAN_TABLES; i++)
            FRAME_TRACE( "        Id = %d, DataSize = %d\n", HuffmanTables[i].Id, HuffmanTables[i].DataSize );
#endif

    return FrameParserNoError;
}
//}}}
//{{{  ReadStartOfScan
// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read the start of scan header
//
FrameParserStatus_t   FrameParser_VideoMjpeg_c::ReadStartOfScan( void )
{
    int                 DataSize;
    unsigned char       Temporary;

    DataSize                            = (int)GetShort() - 2;

    StartOfScan.NumberOfComponents      = GetByte();

    if (StartOfScan.NumberOfComponents != StartOfFrame.NumberOfComponents)
    {
        FRAME_ERROR("%s - Frame and Scan headers have differing numbers of components (%d, %d)\n", __FUNCTION__, StartOfFrame.NumberOfComponents, StartOfScan.NumberOfComponents);
        return FrameParserError;
    }

    for (int i=0; i<StartOfScan.NumberOfComponents; i++)
    {
        StartOfScan.Components[i].Id                    = GetByte();
        Temporary                                       = GetByte();
        StartOfScan.Components[i].HuffmanACIndex        = Temporary & 0xf;
        StartOfScan.Components[i].HuffmanDCIndex        = Temporary >> 4;
    }

    StartOfScan.StartOfSpectralSelection                = GetByte();
    StartOfScan.EndOfSpectralSelection                  = GetByte();
    Temporary                                           = GetByte();
    StartOfScan.ApproximationBitPositionLow             = Temporary & 0xf;
    StartOfScan.ApproximationBitPositionHigh            = Temporary >> 4;

#ifdef DUMP_HEADERS
    FRAME_TRACE( "    MJPEG_SOS  (Start of scan)\n" );
    FRAME_TRACE( "        NumberOfComponents  %d\n", StartOfScan.NumberOfComponents );
    for (int i=0; i<StartOfScan.NumberOfComponents; i++)
        FRAME_TRACE( "            Id = %d, Huffman DC table = %d, AC table = %d\n",
                        StartOfScan.Components[i].Id,
                        StartOfScan.Components[i].HuffmanDCIndex,
                        StartOfScan.Components[i].HuffmanACIndex );
    FRAME_TRACE( "        StartSpectral       %02x\n", StartOfScan.StartOfSpectralSelection );
    FRAME_TRACE( "        EndSpectral         %02x\n", StartOfScan.EndOfSpectralSelection );
    FRAME_TRACE( "        ApproximationHigh   %d\n", StartOfScan.ApproximationBitPositionHigh );
    FRAME_TRACE( "        ApproximationHigh   %d\n", StartOfScan.ApproximationBitPositionLow );
#endif

    return FrameParserNoError;

}
//}}}
#endif
//}}}

//{{{  CommitFrameForDecode
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Send frame for decode
///             On a first slice code, we should have garnered all the data
///             we require we for a frame decode, this function records that fact.
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoMjpeg_c::CommitFrameForDecode (void)
{
    MjpegVideoPictureHeader_t*          PictureHeader;
    MjpegVideoSequence_t*               SequenceHeader;

    FRAME_DEBUG ("%s\n", __FUNCTION__);
    // Check we have the headers we need
    if ((StreamParameters == NULL) || !StreamParameters->SequenceHeaderPresent)
    {
        FRAME_ERROR("%s - Stream parameters unavailable for decode.\n", __FUNCTION__);
        return FrameParserNoStreamParameters;
    }

    if ((FrameParameters == NULL) || !FrameParameters->PictureHeaderPresent)
    {
        FRAME_ERROR("%s - Frame parameters unavailable for decode.\n", __FUNCTION__);
        return FrameParserPartialFrameParameters;
    }

    SequenceHeader              = &StreamParameters->SequenceHeader;
    PictureHeader               = &FrameParameters->PictureHeader;

    // Nick added this to make mjpeg struggle through
    ParsedFrameParameters->FirstParsedParametersForOutputFrame          = FirstDecodeOfFrame;
    ParsedFrameParameters->FirstParsedParametersAfterInputJump          = FirstDecodeAfterInputJump;
    ParsedFrameParameters->SurplusDataInjected                          = SurplusDataInjected;
    ParsedFrameParameters->ContinuousReverseJump                        = ContinuousReverseJump;

    //
    // Record the stream and frame parameters into the appropriate structure
    //

    ParsedFrameParameters->KeyFrame                             = true;
    ParsedFrameParameters->ReferenceFrame                       = false;
    ParsedFrameParameters->IndependentFrame                     = true;

    ParsedFrameParameters->NewStreamParameters                  = false;
    StreamParameters->UpdatedSinceLastFrame                     = false;
    ParsedFrameParameters->SizeofStreamParameterStructure       = sizeof(MjpegStreamParameters_t);
    ParsedFrameParameters->StreamParameterStructure             = StreamParameters;

    ParsedFrameParameters->NewFrameParameters                   = true;
    ParsedFrameParameters->SizeofFrameParameterStructure        = sizeof(MjpegFrameParameters_t);
    ParsedFrameParameters->FrameParameterStructure              = FrameParameters;

    ParsedVideoParameters->Content.Width                        = PictureHeader->frame_width;
    ParsedVideoParameters->Content.Height                       = PictureHeader->frame_height;

    ParsedVideoParameters->Content.FrameRate                    = Rational_t(SequenceHeader->time_scale, SequenceHeader->time_delta);

    ParsedVideoParameters->Content.VideoFullRange               = false;
    ParsedVideoParameters->Content.ColourMatrixCoefficients     = MatrixCoefficients_ITU_R_BT601;

    ParsedVideoParameters->Content.Progressive                  = true;
    ParsedVideoParameters->Content.OverscanAppropriate          = false;

    ParsedVideoParameters->Content.PixelAspectRatio             = 1;

    ParsedVideoParameters->PictureStructure                     = StructureFrame;
    ParsedVideoParameters->InterlacedFrame                      = false;

    ParsedVideoParameters->DisplayCount[0]                      = 1;
    ParsedVideoParameters->DisplayCount[1]                      = 0;

    ParsedVideoParameters->PanScan.Count                        = 0;

    //
    // Record our claim on both the frame and stream parameters
    //

    Buffer->AttachBuffer (StreamParametersBuffer);
    Buffer->AttachBuffer (FrameParametersBuffer);

    //
    // We clear the FrameParameters pointer, a new one will be obtained
    // before/if we read in headers pertaining to the next frame. This
    // will generate an error should I accidentally write code that 
    // accesses this data when it should not.
    //

    FrameParameters             = NULL;

    //
    // Finally set the appropriate flag and return
    //

    FrameToDecode               = true;
    //FrameToDecode               = PictureHeader->picture_coding_type != MJPEG_PICTURE_CODING_TYPE_B;

    return FrameParserNoError;
}
//}}}

