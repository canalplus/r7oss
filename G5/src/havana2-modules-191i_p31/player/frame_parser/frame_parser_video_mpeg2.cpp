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

Source file name : frame_parser_video_mpeg2.cpp
Author :           Nick

Implementation of the mpeg2 video frame parser class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
01-Dec-06   Created                                         Nick

************************************************************************/

//#define DUMP_HEADERS

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "frame_parser_video_mpeg2.h"
#include "ring_generic.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

static BufferDataDescriptor_t     Mpeg2StreamParametersBuffer = BUFFER_MPEG2_STREAM_PARAMETERS_TYPE;
static BufferDataDescriptor_t     Mpeg2FrameParametersBuffer = BUFFER_MPEG2_FRAME_PARAMETERS_TYPE;

//

static struct Count_s
{
    bool                LegalValue;
    unsigned int        PanScanCountValue;
    unsigned int        DisplayCount0Value;
    unsigned int        DisplayCount1Value;
} Counts[] = 
{
				// ProgSeq  Frame   TFF   RFF
    { true,  1, 1, 0 },         //    0       0      0     0
    { false, 0, 0, 0 },         //    0       0      0     1    // Not legal not seen
    { true,  1, 1, 0 },         //    0       0      1     0
    { false, 0, 0, 0 },         //    0       0      1     1    // Not legal not seen
    { true,  2, 1, 1 },         //    0       1      0     0
    { true,  3, 2, 1 },         //    0       1      0     1
    { true,  2, 1, 1 },         //    0       1      1     0
    { true,  3, 2, 1 },         //    0       1      1     1
    { false, 0, 0, 0 },         //    1       0      0     0    // Not legal not seen
    { false, 0, 0, 0 },         //    1       0      0     1    // Not legal not seen
    { false, 0, 0, 0 },         //    1       0      1     0    // Not legal not seen
    { false, 0, 0, 0 },         //    1       0      1     1    // Not legal not seen
    { true,  1, 1, 0 },         //    1       1      0     0
    { true,  2, 2, 0 },         //    1       1      0     1
    { true,  1, 1, 0 },         //    1       1      1     0    // Not legal but seen in actual use so we allow it
    { true,  3, 3, 0 }          //    1       1      1     1
};

#define CountsIndex( PS, F, TFF, RFF )          ((((PS) != 0) << 3) | (((F) != 0) << 2) | (((TFF) != 0) << 1) | ((RFF) != 0))
#define Legal( PS, F, TFF, RFF )                Counts[CountsIndex( PS, F, TFF, RFF )].LegalValue
#define PanScanCount( PS, F, TFF, RFF )         Counts[CountsIndex( PS, F, TFF, RFF )].PanScanCountValue
#define DisplayCount0( PS, F, TFF, RFF )        Counts[CountsIndex( PS, F, TFF, RFF )].DisplayCount0Value
#define DisplayCount1( PS, F, TFF, RFF )        Counts[CountsIndex( PS, F, TFF, RFF )].DisplayCount1Value

//

#define MIN_LEGAL_MPEG2_ASPECT_RATIO_CODE       1
#define MAX_LEGAL_MPEG2_ASPECT_RATIO_CODE       4

// BEWARE !!!! you cannot declare static initializers of a constructed type such as Rational_t
//             the compiler will silently ignore them..........
// Convert all aspect ratios to w/h to be consistent with all other content
#if 0
static unsigned int     Mpeg2AspectRatioValues[][2]     = 
    { 
	{0,1},
	{1,1},
	{3, 4 },
	{ 9, 16 },
	{ 100, 221 } };
#else
static unsigned int     Mpeg2AspectRatioValues[][2]     = 
    { 
	{   1,   0 },
	{   1,   1 },
	{   4,   3 },
	{  16,   9 },
	{ 221, 100 } 
    };
#endif

#define Mpeg2AspectRatios(N) Rational_t(Mpeg2AspectRatioValues[N][0],Mpeg2AspectRatioValues[N][1])

//

#define MIN_LEGAL_MPEG1_ASPECT_RATIO_CODE       1
#define MAX_LEGAL_MPEG1_ASPECT_RATIO_CODE       14

// BEWARE !!!! you cannot declare static initializers of a constructed type such as Rational_t
//             the compiler will silently ignore them..........
static unsigned int     Mpeg1AspectRatioValues[][2]     = 
    { 
	{0,1},
	{10000, 10000},
	{ 6735, 10000},
	{ 7031, 10000},
	{ 7615, 10000},
	{ 8055, 10000},
	{ 8437, 10000},
	{ 8935, 10000},
	{ 9157, 10000},
	{ 9815, 10000},
	{10255, 10000},
	{10695, 10000},
	{10950, 10000},
	{11575, 10000},
	{12015, 10000}
    };

#define Mpeg1AspectRatios(N) Rational_t(Mpeg1AspectRatioValues[N][0],Mpeg1AspectRatioValues[N][1])

//

#define MIN_LEGAL_FRAME_RATE_CODE               1
#define MAX_LEGAL_FRAME_RATE_CODE               8

static unsigned int     FrameRateValues[][2]    =  
    {
	{0,1},
	{ 24000, 1001 },
	{ 24, 1 },
	{ 25, 1 },
	{ 30000, 1001 },
	{ 30, 1 },
	{ 50, 1 },
	{ 60000, 1001 },
	{ 60, 1 }
    };

#define FrameRates(N) Rational_t(FrameRateValues[N][0],FrameRateValues[N][1])

//

static PictureStructure_t       PictureStructures[]     =
    {
	StructureFrame,                 // Strictly illegal
	StructureTopField,
	StructureBottomField,
	StructureFrame
    };

//

static int QuantizationMatrixNaturalOrder[QUANTISER_MATRIX_SIZE] =
{
   0,   1,  8, 16,  9,  2,  3, 10,                              // This translates zigzag matrix indices,
   17, 24, 32, 25, 18, 11,  4,  5,                              // used in coefficient transmission,
   12, 19, 26, 33, 40, 48, 41, 34,                              // to natural order indices.
   27, 20, 13,  6,  7, 14, 21, 28,
   35, 42, 49, 56, 57, 50, 43, 36,
   29, 22, 15, 23, 30, 37, 44, 51,
   58, 59, 52, 45, 38, 31, 39, 46,
   53, 60, 61, 54, 47, 55, 62, 63,
};

//

static SliceType_t SliceTypeTranslation[]  = { INVALID_INDEX, SliceTypeI, SliceTypeP, SliceTypeB, SliceTypeB };

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined macros
//

#define REFERENCE_FRAMES_NEEDED( CodingType )           (CodingType - 1)
#define MAX_REFERENCE_FRAMES_FORWARD_PLAY               REFERENCE_FRAMES_NEEDED(MPEG2_PICTURE_CODING_TYPE_B)

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

// /////////////////////////////////////////////////////////////////////////
//
//      Constructor
//

FrameParser_VideoMpeg2_c::FrameParser_VideoMpeg2_c( void )
{
    Configuration.FrameParserName               = "Unspecified";

    Configuration.StreamParametersCount         = 32;
    Configuration.StreamParametersDescriptor    = &Mpeg2StreamParametersBuffer;

    Configuration.FrameParametersCount          = 32;
    Configuration.FrameParametersDescriptor     = &Mpeg2FrameParametersBuffer;

//

    Reset();
}

// /////////////////////////////////////////////////////////////////////////
//
//      Destructor
//

FrameParser_VideoMpeg2_c::~FrameParser_VideoMpeg2_c( void )
{
    Halt();
    Reset();
}

// /////////////////////////////////////////////////////////////////////////
//
//      The Reset function release any resources, and reset all variable
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::Reset(  void )
{
    memset( &CopyOfStreamParameters, 0x00, sizeof(Mpeg2StreamParameters_t) );
    memset( &ReferenceFrameList, 0x00, sizeof(ReferenceFrameList_t) );

    StreamParameters                            = NULL;
    FrameParameters                             = NULL;
    DeferredParsedFrameParameters               = NULL;
    DeferredParsedVideoParameters               = NULL;

    FirstDecodeOfFrame                          = true;
    EverSeenRepeatFirstField                    = false;

    LastFirstFieldWasAnI                        = false;

//

    return FrameParser_Video_c::Reset();
}


// /////////////////////////////////////////////////////////////////////////
//
//      The register output ring function
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::RegisterOutputBufferRing(       Ring_t          Ring )
{
PlayerStatus_t  Status;

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

    Status      = FrameParser_Video_c::RegisterOutputBufferRing( Ring );
    if( Status != FrameParserNoError )
	return Status;

    //
    // Pass the call down the line
    //

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The read headers stream specific function
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::ReadHeaders( void )
{
unsigned int            i;
unsigned int            Code;
FrameParserStatus_t     Status;
unsigned int            ExtensionCode;
bool                    IgnoreStreamComponents;
bool                    IgnoreFrameComponents;

//

    IgnoreFrameComponents               = true;
    IgnoreStreamComponents              = true;

//

    for( i=0; i<StartCodeList->NumberOfStartCodes; i++ )
    {
	Code    = StartCodeList->StartCodes[i];
	Bits.SetPointer( BufferData + ExtractStartCodeOffset(Code) );
	Bits.FlushUnseen(32);

	Status  = FrameParserNoError;
	switch( ExtractStartCodeCode(Code) )
	{
	    case   MPEG2_PICTURE_START_CODE:
			Status  = ReadPictureHeader();
			IgnoreFrameComponents   = (Status != FrameParserNoError);
			break;

	    case   MPEG2_FIRST_SLICE_START_CODE:
			if( IgnoreFrameComponents )
			    break;

			ParsedFrameParameters->DataOffset       = ExtractStartCodeOffset(Code);
			Status  = CommitFrameForDecode();

			IgnoreFrameComponents                   = true;
			break;

	    case   MPEG2_USER_DATA_START_CODE:
			break;

	    case   MPEG2_SEQUENCE_HEADER_CODE:
			Status   = ReadSequenceHeader();
			IgnoreStreamComponents  = (Status != FrameParserNoError);
			break;

	    case   MPEG2_SEQUENCE_ERROR_CODE:
			break;

	    case   MPEG2_EXTENSION_START_CODE:
			ExtensionCode = Bits.Get(4);
			switch( ExtensionCode )
			{
			    case   MPEG2_SEQUENCE_EXTENSION_ID:
					if( IgnoreStreamComponents )
					    break;

					Status   = ReadSequenceExtensionHeader();
					break;

			    case   MPEG2_SEQUENCE_DISPLAY_EXTENSION_ID:
					if( IgnoreStreamComponents )
					    break;

					Status   = ReadSequenceDisplayExtensionHeader();
					break;

			    case   MPEG2_QUANT_MATRIX_EXTENSION_ID:
					Status   = ReadQuantMatrixExtensionHeader();
					break;

			    case   MPEG2_SEQUENCE_SCALABLE_EXTENSION_ID:
					if( IgnoreStreamComponents )
					    break;

					Status   = ReadSequenceScalableExtensionHeader();
					break;

			    case   MPEG2_PICTURE_DISPLAY_EXTENSION_ID:
					if( IgnoreFrameComponents )
					    break;

					Status   = ReadPictureDisplayExtensionHeader();

					if( Status != FrameParserNoError )
					    IgnoreFrameComponents       = true;
					break;

			    case   MPEG2_PICTURE_CODING_EXTENSION_ID:
					if( IgnoreFrameComponents )
					    break;

					Status   = ReadPictureCodingExtensionHeader();
					break;

			    case   MPEG2_PICTURE_SPATIAL_SCALABLE_EXTENSION_ID:
					if( IgnoreFrameComponents )
					    break;

					Status   = ReadPictureSpatialScalableExtensionHeader();
					break;

			    case   MPEG2_PICTURE_TEMPORAL_SCALABLE_EXTENSION_ID:
					if( IgnoreFrameComponents )
					    break;

					Status   = ReadPictureTemporalScalableExtensionHeader();
					break;

			    default:
					report( severity_error, "FrameParser_VideoMpeg2_c::ReadHeaders - Unknown/Unsupported extension header %02x\n", ExtensionCode );
					Status  = FrameParserUnhandledHeader;
					break;
			}

			if( (Status == FrameParserNoError) && (StreamParameters != NULL) )
			    StreamParameters->StreamType        = MpegStreamTypeMpeg2;
			break;

	    case   MPEG2_SEQUENCE_END_CODE:
			break;

	    case   MPEG2_GROUP_START_CODE:
			if( IgnoreStreamComponents )
			    break;

			Status  = ReadGroupOfPicturesHeader();
			break;

	    case   MPEG2_VIDEO_PES_START_CODE:
			report( severity_error, "FrameParser_VideoMpeg2_c::ReadHeaders - Pes headers should be removed\n" );
			Status  = FrameParserUnhandledHeader;
			break;

	    default:
			report( severity_error, "FrameParser_VideoMpeg2_c::ReadHeaders - Unknown/Unsupported header %02x\n", ExtractStartCodeCode(Code) );
			Status  = FrameParserUnhandledHeader;
			break;
	}

	if( (Status != FrameParserNoError) && (Status != FrameParserUnhandledHeader) )
	{
	    IgnoreStreamComponents      = true;
	    IgnoreFrameComponents       = true;
	}
    }

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The reset reference frame list function
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::ResetReferenceFrameList( void )
{
    Player->CallInSequence( Stream, SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, CODEC_RELEASE_ALL );

    ReferenceFrameList.EntryCount       = 0;

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//      Stream specific override function for processing decode stacks, this 
//      initializes the post decode ring before passing into the real 
//      implementation of this function.
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::RevPlayProcessDecodeStacks( void )
{
    ReverseQueuedPostDecodeSettingsRing->Flush();

    return FrameParser_Video_c::RevPlayProcessDecodeStacks();
}


// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to generate the post decode parameter 
//      settings for reverse play, these consist of the display frame index,
//      and presentation time, both of which may be deferred if the information 
//      is unavailable.
//
//      For mpeg2 reverse play, this function will use a simple numbering system,
//      Imaging a sequence  I B B P B B this should be numbered (in reverse) as
//                          3 5 4 0 2 1
//      These will be presented to this function in reverse order ( B B P B B I )
//      so for non ref frames we ring them, and for ref frames we use the next number
//      and then process what is on the ring.
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::RevPlayGeneratePostDecodeParameterSettings( void )
{
    //
    // If this is the first decode of a frame then we need display frame indices and presentation timnes
    //

    if( ParsedFrameParameters->FirstParsedParametersForOutputFrame )
    {
	//
	// If this is not a reference frame then place it on the ring for calculation later
	//

	if( !ParsedFrameParameters->ReferenceFrame )
	{
	    ReverseQueuedPostDecodeSettingsRing->Insert( (unsigned int)ParsedFrameParameters );
	    ReverseQueuedPostDecodeSettingsRing->Insert( (unsigned int)ParsedVideoParameters );
	}
	else

	//
	// If this is a reference frame then first process it, then process the frames on the ring
	//

	{
	    CalculateFrameIndexAndPts( ParsedFrameParameters, ParsedVideoParameters );

	    while( ReverseQueuedPostDecodeSettingsRing->NonEmpty() )
	    {
		ReverseQueuedPostDecodeSettingsRing->Extract( (unsigned int *)&DeferredParsedFrameParameters );
		ReverseQueuedPostDecodeSettingsRing->Extract( (unsigned int *)&DeferredParsedVideoParameters );
		CalculateFrameIndexAndPts( DeferredParsedFrameParameters, DeferredParsedVideoParameters );
	    }
	}
    }

//

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to prepare a reference frame list
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::PrepareReferenceFrameList( void )
{
unsigned int    i;
bool            Field;
bool            SecondField;
bool            FirstField;
bool            SelfReference;
unsigned int    ReferenceFramesNeeded;
unsigned int    PictureCodingType;

    //
    // Note we cannot use StreamParameters or FrameParameters to address data directly, 
    // as these may no longer apply to the frame we are dealing with.
    // Particularly if we have seen a sequenece header or group of pictures 
    // header which belong to the next frame.
    //

    PictureCodingType           = ((Mpeg2FrameParameters_t *)(ParsedFrameParameters->FrameParameterStructure))->PictureHeader.picture_coding_type;
    ReferenceFramesNeeded       = REFERENCE_FRAMES_NEEDED(PictureCodingType);

    //
    // Detect the special case of a second field referencing the first
    // this is when we have the field startup condition
    //          I P P P 
    // where the first P actually references its own decode buffer.
    //

    Field                       = (ParsedVideoParameters->PictureStructure != StructureFrame);
    SecondField                 = Field && !ParsedFrameParameters->FirstParsedParametersForOutputFrame;
    FirstField                  = Field && ParsedFrameParameters->FirstParsedParametersForOutputFrame;

    SelfReference               = SecondField && (ReferenceFramesNeeded == 1) && LastFirstFieldWasAnI;

    if( FirstField )
	LastFirstFieldWasAnI    = (PictureCodingType == MPEG2_PICTURE_CODING_TYPE_I);

    //
    // Now we cannot decode if we do not have enbough reference frames, 
    // and this is not the most heinous of special cases.
    //

    if( !SelfReference && (ReferenceFrameList.EntryCount < ReferenceFramesNeeded) )
	return FrameParserInsufficientReferenceFrames;

//

    ParsedFrameParameters->NumberOfReferenceFrameLists                  = 1;
    ParsedFrameParameters->ReferenceFrameList[0].EntryCount             = ReferenceFramesNeeded;

    if( SelfReference )
    {
	ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[0]   = NextDecodeFrameIndex;
    }
    else
    {
	for( i=0; i<ReferenceFramesNeeded; i++ )
	    ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[i]   = ReferenceFrameList.EntryIndicies[ReferenceFrameList.EntryCount - ReferenceFramesNeeded + i];
    }

//

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to manage a reference frame list in forward play
//      we only record a reference frame as such on the last field, in order to 
//      ensure the correct management of reference frames in the codec, we immediately 
//      inform the codec of a release on the first field of a field picture.
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::ForPlayUpdateReferenceFrameList( void )
{
unsigned int    i;
bool            LastField;

//

    if( ParsedFrameParameters->ReferenceFrame )
    {
	LastField       = (ParsedVideoParameters->PictureStructure == StructureFrame) || 
			  !ParsedFrameParameters->FirstParsedParametersForOutputFrame;

	if( LastField )
	{
	    if( ReferenceFrameList.EntryCount >= MAX_REFERENCE_FRAMES_FORWARD_PLAY )
	    {
		Player->CallInSequence( Stream, SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, ReferenceFrameList.EntryIndicies[0] );

		ReferenceFrameList.EntryCount--;
		for( i=0; i<ReferenceFrameList.EntryCount; i++ )
		    ReferenceFrameList.EntryIndicies[i] = ReferenceFrameList.EntryIndicies[i+1];
	    }

	    ReferenceFrameList.EntryIndicies[ReferenceFrameList.EntryCount++] = ParsedFrameParameters->DecodeFrameIndex;
	}
	else
	{
	    Player->CallInSequence( Stream, SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, ParsedFrameParameters->DecodeFrameIndex );
	}
    }

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to add a frame to the reference 
//      frame list in reverse play.
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::RevPlayAppendToReferenceFrameList( void )
{
bool            LastField;

//

    LastField   = (ParsedVideoParameters->PictureStructure == StructureFrame) || 
		  !ParsedFrameParameters->FirstParsedParametersForOutputFrame;

    if( ParsedFrameParameters->ReferenceFrame && LastField )
    {
	if( ReferenceFrameList.EntryCount >= MAX_ENTRIES_IN_REFERENCE_FRAME_LIST )
	{
	    report( severity_error, "FrameParser_VideoMpeg2_c::RevPlayAppendToReferenceFrameList - List full - Implementation error.\n" );
	    return PlayerImplementationError;
	}

	ReferenceFrameList.EntryIndicies[ReferenceFrameList.EntryCount++] = ParsedFrameParameters->DecodeFrameIndex;
    }

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to remove a frame from the reference 
//      frame list in reverse play.
//
//      Note, we only inserted the reference frame in the list on the last 
//      field but we need to inform the codec we are finished with it on both
//      fields (for field pictures).
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::RevPlayRemoveReferenceFrameFromList( void )
{
bool            LastField;

//

    LastField   = (ParsedVideoParameters->PictureStructure == StructureFrame) || 
		  !ParsedFrameParameters->FirstParsedParametersForOutputFrame;

    if( (ReferenceFrameList.EntryCount != 0) && LastField )
    {
	Player->CallInSequence( Stream, SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, ParsedFrameParameters->DecodeFrameIndex );

	if( LastField )
	    ReferenceFrameList.EntryCount--;
    }

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to junk the reference frame list
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::RevPlayJunkReferenceFrameList( void )
{
    ReferenceFrameList.EntryCount       = 0;
    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in a sequence header
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::ReadSequenceHeader( void )
{
int                                      i;
FrameParserStatus_t                      Status;
Mpeg2VideoSequence_t                    *Header;
Mpeg2VideoQuantMatrixExtension_t        *CumulativeHeader;

//

    Status      = GetNewStreamParameters( (void **)&StreamParameters );
    if( Status != FrameParserNoError )
	return Status;

    StreamParameters->UpdatedSinceLastFrame     = true;
    Header                              = &StreamParameters->SequenceHeader;
    memset( Header, 0x00, sizeof(Mpeg2VideoSequence_t) );

//

    Header->horizontal_size_value                       = Bits.Get(12);
    Header->vertical_size_value                         = Bits.Get(12);
    Header->aspect_ratio_information                    = Bits.Get(4);
    Header->frame_rate_code                             = Bits.Get(4);
    Header->bit_rate_value                              = Bits.Get(18);
    MarkerBit(1);
    Header->vbv_buffer_size_value                       = Bits.Get(10);
    Header->constrained_parameters_flag                 = Bits.Get(1);
    Header->load_intra_quantizer_matrix                 = Bits.Get(1);

    if( Header->load_intra_quantizer_matrix )
	for( i=0; i<64; i++ )
	    Header->intra_quantizer_matrix[QuantizationMatrixNaturalOrder[i]]   = Bits.Get(8);

    Header->load_non_intra_quantizer_matrix             = Bits.Get(1);

    if( Header->load_non_intra_quantizer_matrix )
	for( i=0; i<64; i++ )
	    Header->non_intra_quantizer_matrix[QuantizationMatrixNaturalOrder[i]]= Bits.Get(8);

//

    CumulativeHeader    = &StreamParameters->CumulativeQuantMatrices;

    if( Header->load_intra_quantizer_matrix )
    {
	CumulativeHeader->load_intra_quantizer_matrix           = Header->load_intra_quantizer_matrix;
	memcpy( CumulativeHeader->intra_quantizer_matrix, Header->intra_quantizer_matrix, sizeof(QuantiserMatrix_t) );
    }

    if( Header->load_non_intra_quantizer_matrix )
    {
	CumulativeHeader->load_non_intra_quantizer_matrix       = Header->load_non_intra_quantizer_matrix;
	memcpy( CumulativeHeader->non_intra_quantizer_matrix, Header->non_intra_quantizer_matrix, sizeof(QuantiserMatrix_t) );
    }

//

    if( !inrange(Header->frame_rate_code, MIN_LEGAL_FRAME_RATE_CODE, MAX_LEGAL_FRAME_RATE_CODE) )
    {
	report( severity_error, "FrameParser_VideoMpeg2_c::ReadSequenceHeader - frame_rate_code has illegal value (%02x).\n", Header->frame_rate_code );
	return FrameParserHeaderSyntaxError;
    }

//

    StreamParameters->SequenceHeaderPresent             = true;

    //
    // Reset the last pan scan offsets
    //

    LastPanScanHorizontalOffset         = 0;
    LastPanScanVerticalOffset           = 0;

//

#ifdef DUMP_HEADERS
    report( severity_info, "Sequence header :- \n" );
    report( severity_info, "    horizontal_size_value        : %6d\n", Header->horizontal_size_value );
    report( severity_info, "    vertical_size_value          : %6d\n", Header->vertical_size_value );
    report( severity_info, "    aspect_ratio_information     : %6d\n", Header->aspect_ratio_information );
    report( severity_info, "    frame_rate_code              : %6d\n", Header->frame_rate_code );
    report( severity_info, "    bit_rate_value               : %6d\n", Header->bit_rate_value );
    report( severity_info, "    vbv_buffer_size_value        : %6d\n", Header->vbv_buffer_size_value );

    report( severity_info, "    load_intra_quantizer_matrix  : %6d\n", Header->load_intra_quantizer_matrix );
    if( Header->load_intra_quantizer_matrix )
	for( i=0; i<64; i+=16 )
	{
	int j;
	    report( severity_info, "            " );
	    for( j=0; j<16; j++ )
		report( severity_info, "%02x ", Header->intra_quantizer_matrix[i+j] );
	    report( severity_info, "\n" );
	}

    report( severity_info, "    load_non_intra_quantizer_matrix  : %6d\n", Header->load_non_intra_quantizer_matrix );
    if( Header->load_non_intra_quantizer_matrix )
	for( i=0; i<64; i+=16 )
	{
	int j;
	    report( severity_info, "            " );
	    for( j=0; j<16; j++ )
		report( severity_info, "%02x ", Header->non_intra_quantizer_matrix[i+j] );
	    report( severity_info, "\n" );
	}
#endif

    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in a sequence extension header
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::ReadSequenceExtensionHeader( void )
{
Mpeg2VideoSequenceExtension_t   *Header;

//

    if( (StreamParameters == NULL) || !StreamParameters->SequenceHeaderPresent )
    {
	report( severity_error, "FrameParser_VideoMpeg2_c::ReadSequenceExtensionHeader - Appropriate sequence header not found.\n" );
	return FrameParserNoStreamParameters;
    }

    Header      = &StreamParameters->SequenceExtensionHeader;
    memset( Header, 0x00, sizeof(Mpeg2VideoSequenceExtension_t) );

//

    Header->profile_and_level_indication        = Bits.Get(8);
    Header->progressive_sequence                = Bits.Get(1);
    Header->chroma_format                       = Bits.Get(2);
    Header->horizontal_size_extension           = Bits.Get(2);
    Header->vertical_size_extension             = Bits.Get(2);
    Header->bit_rate_extension                  = Bits.Get(12);
    MarkerBit(1);
    Header->vbv_buffer_size_extension           = Bits.Get(8);
    Header->low_delay                           = Bits.Get(1);
    Header->frame_rate_extension_n              = Bits.Get(2);
    Header->frame_rate_extension_d              = Bits.Get(5);

//

    if( Header->chroma_format != MPEG2_SEQUENCE_CHROMA_4_2_0 )
    {
	report( severity_error, "FrameParser_VideoMpeg2_c::ReadSequenceExtensionHeader - Unsupported chroma format %d.\n", Header->chroma_format );
	Player->MarkStreamUnPlayable( Stream );
	return FrameParserHeaderUnplayable;
    }

    StreamParameters->SequenceExtensionHeaderPresent    = true;

//

#ifdef DUMP_HEADERS
    report( severity_info, "Sequence Extension header :- \n" );
    report( severity_info, "    profile_and_level_indication : %6d\n", Header->profile_and_level_indication );
    report( severity_info, "    progressive_sequence         : %6d\n", Header->progressive_sequence );
    report( severity_info, "    chroma_format                : %6d\n", Header->chroma_format );
    report( severity_info, "    horizontal_size_extension    : %6d\n", Header->horizontal_size_extension );
    report( severity_info, "    vertical_size_extension      : %6d\n", Header->vertical_size_extension );
    report( severity_info, "    bit_rate_extension           : %6d\n", Header->bit_rate_extension );
    report( severity_info, "    vbv_buffer_size_extension    : %6d\n", Header->vbv_buffer_size_extension );
    report( severity_info, "    low_delay                    : %6d\n", Header->low_delay );
    report( severity_info, "    frame_rate_extension_n       : %6d\n", Header->frame_rate_extension_n );
    report( severity_info, "    frame_rate_extension_d       : %6d\n", Header->frame_rate_extension_d );
#endif

    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in a sequence display extension header
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::ReadSequenceDisplayExtensionHeader( void )
{
Mpeg2VideoSequenceDisplayExtension_t    *Header;

//

    if( (StreamParameters == NULL) || !StreamParameters->SequenceHeaderPresent )
    {
	report( severity_error, "FrameParser_VideoMpeg2_c::ReadSequenceDisplayExtensionHeader - Appropriate sequence header not found.\n" );
	return FrameParserNoStreamParameters;
    }

    Header      = &StreamParameters->SequenceDisplayExtensionHeader;
    memset( Header, 0x00, sizeof(Mpeg2VideoSequenceDisplayExtension_t) );

//

    Header->video_format                      = Bits.Get(3);
    Header->color_description                 = Bits.Get(1);
    if(  Header->color_description != 0 )
    {
	Header->color_primaries               = Bits.Get(8);
	Header->transfer_characteristics      = Bits.Get(8);
	Header->matrix_coefficients           = Bits.Get(8);
    }
    Header->display_horizontal_size           = Bits.Get(14);
    MarkerBit(1);
    Header->display_vertical_size             = Bits.Get(14);

//

    StreamParameters->SequenceDisplayExtensionHeaderPresent     = true;

//

#ifdef DUMP_HEADERS
    report( severity_info, "Sequence Display Extension header :- \n" );
    report( severity_info, "    video_format                 : %6d\n", Header->video_format );
    if( Header->color_description != 0 )
    {
	report( severity_info, "    color_primaries              : %6d\n", Header->color_primaries );
	report( severity_info, "    transfer_characteristics     : %6d\n", Header->transfer_characteristics );
	report( severity_info, "    matrix_coefficients          : %6d\n", Header->matrix_coefficients );
    }
    report( severity_info, "    display_horizontal_size      : %6d\n", Header->display_horizontal_size );
    report( severity_info, "    display_vertical_size        : %6d\n", Header->display_vertical_size );
#endif

    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in a sequence scalable extension header
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::ReadSequenceScalableExtensionHeader( void )
{
Mpeg2VideoSequenceScalableExtension_t   *Header;

//

    if( (StreamParameters == NULL) || !StreamParameters->SequenceHeaderPresent )
    {
	report( severity_error, "FrameParser_VideoMpeg2_c::ReadSequenceScalableExtensionHeader - Appropriate sequence header not found.\n" );
	return FrameParserNoStreamParameters;
    }

    Header      = &StreamParameters->SequenceScalableExtensionHeader;
    memset( Header, 0x00, sizeof(Mpeg2VideoSequenceScalableExtension_t) );

//

    Header->scalable_mode                               = Bits.Get(2);
    Header->layer_id                                    = Bits.Get(4);

    if( Header->scalable_mode == MPEG2_SCALABLE_MODE_SPATIAL_SCALABILITY )
    {
	Header->lower_layer_prediction_horizontal_size  = Bits.Get(14);
	MarkerBit(1);
	Header->lower_layer_prediction_vertical_size    = Bits.Get(14);
	Header->horizontal_subsampling_factor_m         = Bits.Get(5);
	Header->horizontal_subsampling_factor_n         = Bits.Get(5);
	Header->vertical_subsampling_factor_m           = Bits.Get(5);
	Header->vertical_subsampling_factor_n           = Bits.Get(5);
    }
    else if( Header->scalable_mode == MPEG2_SCALABLE_MODE_TEMPORAL_SCALABILITY )
    {
	Header->picture_mux_enable                      = Bits.Get(1);
	if( Header->picture_mux_enable )
	{
	    Header->mux_to_progressive_sequence         = Bits.Get(1);
	    Header->picture_mux_order                   = Bits.Get(3);
	    Header->picture_mux_factor                  = Bits.Get(3);
	}
    }

//

    StreamParameters->SequenceScalableExtensionHeaderPresent    = true;

//

#ifdef DUMP_HEADERS
    report( severity_info, "Sequence Scalable Extension header :- \n" );
    report( severity_info, "    scalable_mode                : %6d\n", Header->scalable_mode );
    report( severity_info, "    layer_id                     : %6d\n", Header->layer_id );
    if( Header->scalable_mode == MPEG2_SCALABLE_MODE_SPATIAL_SCALABILITY )
    {
	report( severity_info, "    lower_layer_prediction_horizontal_size : %6d\n", Header->lower_layer_prediction_horizontal_size );
	report( severity_info, "    lower_layer_prediction_vertical_size   : %6d\n", Header->lower_layer_prediction_vertical_size );
	report( severity_info, "    horizontal_subsampling_factor_m        : %6d\n", Header->horizontal_subsampling_factor_m );
	report( severity_info, "    horizontal_subsampling_factor_n        : %6d\n", Header->horizontal_subsampling_factor_n );
	report( severity_info, "    vertical_subsampling_factor_m          : %6d\n", Header->vertical_subsampling_factor_m );
	report( severity_info, "    vertical_subsampling_factor_n          : %6d\n", Header->vertical_subsampling_factor_n );
    }
    else if( Header->scalable_mode == MPEG2_SCALABLE_MODE_TEMPORAL_SCALABILITY )
    {
	report( severity_info, "    picture_mux_enable           : %6d\n", Header->picture_mux_enable );
	if( Header->picture_mux_enable )
	{
	    report( severity_info, "    mux_to_progressive_sequence  : %6d\n", Header->mux_to_progressive_sequence );
	    report( severity_info, "    picture_mux_order            : %6d\n", Header->picture_mux_order );
	    report( severity_info, "    picture_mux_factor           : %6d\n", Header->picture_mux_factor );
	}
    }
#endif

    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in a group of pictures header
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::ReadGroupOfPicturesHeader( void )
{
FrameParserStatus_t      Status;
Mpeg2VideoGOP_t         *Header;

//

    Status      = GetNewFrameParameters( (void **)&FrameParameters );
    if( Status != FrameParserNoError )
	return Status;

    Header      = &FrameParameters->GroupOfPicturesHeader;
    memset( Header, 0x00, sizeof(Mpeg2VideoGOP_t) );

//

    Header->time_code.drop_frame_flag   = Bits.Get(1);
    Header->time_code.hours             = Bits.Get(5);
    Header->time_code.minutes           = Bits.Get(6);
    MarkerBit(1);
    Header->time_code.seconds           = Bits.Get(6);
    Header->time_code.pictures          = Bits.Get(6);
    Header->closed_gop                  = Bits.Get(1);
    Header->broken_link                 = Bits.Get(1);

//

    FrameParameters->GroupOfPicturesHeaderPresent       = true;

//

#ifdef DUMP_HEADERS
    report( severity_note, "Gop header :- \n" );
    report( severity_note, "    time_code.drop_frame_flag    : %6d\n", Header->time_code.drop_frame_flag );
    report( severity_note, "    time_code.hours              : %6d\n", Header->time_code.hours );
    report( severity_note, "    time_code.minutes            : %6d\n", Header->time_code.minutes );
    report( severity_note, "    time_code.seconds            : %6d\n", Header->time_code.seconds );
    report( severity_note, "    time_code.pictures           : %6d\n", Header->time_code.pictures );
    report( severity_note, "    closed_gop                   : %6d\n", Header->closed_gop );
    report( severity_note, "    broken_link                  : %6d\n", Header->broken_link );
#endif

    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in a picture header
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::ReadPictureHeader( void )
{
FrameParserStatus_t      Status;
Mpeg2VideoPicture_t     *Header;

//

    if( (StreamParameters == NULL) || !StreamParameters->SequenceHeaderPresent )
    {
	report( severity_error, "FrameParser_VideoMpeg2_c::ReadPictureHeader - Appropriate sequence header not found.\n" );
	return FrameParserNoStreamParameters;
    }

//

    if( FrameParameters == NULL )
    {
	Status  = GetNewFrameParameters( (void **)&FrameParameters );
	if( Status != FrameParserNoError )
	    return Status;
    }

    Header      = &FrameParameters->PictureHeader;
    memset( Header, 0x00, sizeof(Mpeg2VideoPicture_t) );

//

    Header->temporal_reference                  = Bits.Get(10);
    Header->picture_coding_type                 = Bits.Get(3);
    Header->vbv_delay                           = Bits.Get(16);

    if( Header->picture_coding_type == MPEG2_PICTURE_CODING_TYPE_P )
    {
	Header->full_pel_forward_vector         = Bits.Get(1);
	Header->forward_f_code                  = Bits.Get(3);
    }
    else if( Header->picture_coding_type == MPEG2_PICTURE_CODING_TYPE_B )
    {
	Header->full_pel_forward_vector         = Bits.Get(1);
	Header->forward_f_code                  = Bits.Get(3);
	Header->full_pel_backward_vector        = Bits.Get(1);
	Header->backward_f_code                 = Bits.Get(3);
    }

//

    FrameParameters->PictureHeaderPresent       = true;

//

#ifdef DUMP_HEADERS
    report( severity_note, "Picture header :- \n" );
    report( severity_note, "    temporal_reference           : %6d\n", Header->temporal_reference );
    report( severity_note, "    picture_coding_type          : %6d\n", Header->picture_coding_type );
    report( severity_note, "    vbv_delay                    : %6d\n", Header->vbv_delay );
    if( (Header->picture_coding_type == MPEG2_PICTURE_CODING_TYPE_P) ||
	(Header->picture_coding_type == MPEG2_PICTURE_CODING_TYPE_B) )
    {
	report( severity_note, "    full_pel_forward_vector      : %6d\n", Header->full_pel_forward_vector );
	report( severity_note, "    forward_f_code               : %6d\n", Header->forward_f_code );
    }
    if( Header->picture_coding_type == MPEG2_PICTURE_CODING_TYPE_B )
    {
	report( severity_note, "    full_pel_backward_vector     : %6d\n", Header->full_pel_backward_vector );
	report( severity_note, "    backward_f_code              : %6d\n", Header->backward_f_code );
    }
#endif

    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in a picture coding extension header
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::ReadPictureCodingExtensionHeader( void )
{
Mpeg2VideoPictureCodingExtension_t      *Header;

//

    if( (FrameParameters == NULL) || !FrameParameters->PictureHeaderPresent )
    {
	report( severity_error, "FrameParser_VideoMpeg2_c::ReadPictureCodingExtensionHeader - Appropriate picture header not found.\n" );
	return FrameParserNoStreamParameters;
    }

    Header      = &FrameParameters->PictureCodingExtensionHeader;
    memset( Header, 0x00, sizeof(Mpeg2VideoPictureCodingExtension_t) );

//

    Header->f_code[0][0]                = Bits.Get(4);
    Header->f_code[0][1]                = Bits.Get(4);
    Header->f_code[1][0]                = Bits.Get(4);
    Header->f_code[1][1]                = Bits.Get(4);
    Header->intra_dc_precision          = Bits.Get(2);
    Header->picture_structure           = Bits.Get(2);
    Header->top_field_first             = Bits.Get(1);
    Header->frame_pred_frame_dct        = Bits.Get(1);
    Header->concealment_motion_vectors  = Bits.Get(1);
    Header->q_scale_type                = Bits.Get(1);
    Header->intra_vlc_format            = Bits.Get(1);
    Header->alternate_scan              = Bits.Get(1);
    Header->repeat_first_field          = Bits.Get(1);
    Header->chroma_420_type             = Bits.Get(1);
    Header->progressive_frame           = Bits.Get(1);
    Header->composite_display_flag      = Bits.Get(1);

    if( Header->composite_display_flag )
    {
	Header->v_axis                      = Bits.Get(1);
	Header->field_sequence              = Bits.Get(3);
	Header->sub_carrier                 = Bits.Get(1);
	Header->burst_amplitude             = Bits.Get(7);
	Header->sub_carrier_phase           = Bits.Get(8);
    }

//

    FrameParameters->PictureCodingExtensionHeaderPresent        = true;

//

#ifdef DUMP_HEADERS
    report( severity_info, "Picture Coding Extension header :- \n" );
    report( severity_info, "    f_code                       : %6d %6d %6d %6d\n", Header->f_code[0][0],
								    Header->f_code[0][1],
								    Header->f_code[1][0],
								    Header->f_code[1][1] );
    report( severity_info, "    intra_dc_precision           : %6d\n", Header->intra_dc_precision );
    report( severity_info, "    picture_structure            : %6d\n", Header->picture_structure );
    report( severity_info, "    top_field_first              : %6d\n", Header->top_field_first );
    report( severity_info, "    frame_pred_frame_dct         : %6d\n", Header->frame_pred_frame_dct );
    report( severity_info, "    concealment_motion_vectors   : %6d\n", Header->concealment_motion_vectors );
    report( severity_info, "    q_scale_type                 : %6d\n", Header->q_scale_type );
    report( severity_info, "    intra_vlc_format             : %6d\n", Header->intra_vlc_format );
    report( severity_info, "    alternate_scan               : %6d\n", Header->alternate_scan );
    report( severity_info, "    repeat_first_field           : %6d\n", Header->repeat_first_field );
    report( severity_info, "    chroma_420_type              : %6d\n", Header->chroma_420_type );
    report( severity_info, "    progressive_frame            : %6d\n", Header->progressive_frame );
    report( severity_info, "    composite_display_flag       : %6d\n", Header->composite_display_flag );
    if( Header->composite_display_flag )
    {
	report( severity_info, "    v_axis                       : %6d\n", Header->v_axis );
	report( severity_info, "    field_sequence               : %6d\n", Header->field_sequence );
	report( severity_info, "    sub_carrier                  : %6d\n", Header->sub_carrier );
	report( severity_info, "    burst_amplitude              : %6d\n", Header->burst_amplitude );
	report( severity_info, "    sub_carrier_phase            : %6d\n", Header->sub_carrier_phase );
    }
#endif

    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in a Quantization matrix extension header
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::ReadQuantMatrixExtensionHeader( void )
{
unsigned int                             i;
Mpeg2VideoQuantMatrixExtension_t        *Header;
Mpeg2VideoQuantMatrixExtension_t        *CumulativeHeader;

//

    if( (StreamParameters == NULL) || !StreamParameters->SequenceHeaderPresent )
    {
	report( severity_error, "FrameParser_VideoMpeg2_c::ReadQuantMatrixExtensionHeader - Appropriate sequence header not found.\n" );
	return FrameParserNoStreamParameters;
    }

    StreamParameters->UpdatedSinceLastFrame     = true;

    Header      = &StreamParameters->QuantMatrixExtensionHeader;
    memset( Header, 0x00, sizeof(Mpeg2VideoQuantMatrixExtension_t) );

//

    Header->load_intra_quantizer_matrix                                                 = Bits.Get(1);
    if( Header->load_intra_quantizer_matrix )
	for( i=0; i<64; i++ )
	    Header->intra_quantizer_matrix[QuantizationMatrixNaturalOrder[i]]           = Bits.Get(8);


    Header->load_non_intra_quantizer_matrix                                             = Bits.Get(1);
    if( Header->load_non_intra_quantizer_matrix )
	for( i=0; i<64; i++ )
	    Header->non_intra_quantizer_matrix[QuantizationMatrixNaturalOrder[i]]       = Bits.Get(8);

    Header->load_chroma_intra_quantizer_matrix                                          = Bits.Get(1);
    if( Header->load_chroma_intra_quantizer_matrix )
	for( i=0; i<64; i++ )
	    Header->chroma_intra_quantizer_matrix[QuantizationMatrixNaturalOrder[i]]    = Bits.Get(8);


    Header->load_chroma_non_intra_quantizer_matrix                                      = Bits.Get(1);
    if( Header->load_chroma_non_intra_quantizer_matrix )
	for( i=0; i<64; i++ )
	    Header->chroma_non_intra_quantizer_matrix[QuantizationMatrixNaturalOrder[i]]= Bits.Get(8);

//

    CumulativeHeader    = &StreamParameters->CumulativeQuantMatrices;

    if( Header->load_intra_quantizer_matrix )
    {
	CumulativeHeader->load_intra_quantizer_matrix           = Header->load_intra_quantizer_matrix;
	memcpy( CumulativeHeader->intra_quantizer_matrix, Header->intra_quantizer_matrix, sizeof(QuantiserMatrix_t) );
    }

    if( Header->load_non_intra_quantizer_matrix )
    {
	CumulativeHeader->load_non_intra_quantizer_matrix       = Header->load_non_intra_quantizer_matrix;
	memcpy( CumulativeHeader->non_intra_quantizer_matrix, Header->non_intra_quantizer_matrix, sizeof(QuantiserMatrix_t) );
    }

    if( Header->load_chroma_intra_quantizer_matrix )
    {
	CumulativeHeader->load_chroma_intra_quantizer_matrix    = Header->load_chroma_intra_quantizer_matrix;
	memcpy( CumulativeHeader->chroma_intra_quantizer_matrix, Header->chroma_intra_quantizer_matrix, sizeof(QuantiserMatrix_t) );
    }

    if( Header->load_chroma_non_intra_quantizer_matrix )
    {
	CumulativeHeader->load_chroma_non_intra_quantizer_matrix= Header->load_chroma_non_intra_quantizer_matrix;
	memcpy( CumulativeHeader->chroma_non_intra_quantizer_matrix, Header->chroma_non_intra_quantizer_matrix, sizeof(QuantiserMatrix_t) );
    }

//

    StreamParameters->QuantMatrixExtensionHeaderPresent = true;

//

#ifdef DUMP_HEADERS
    report( severity_info, "Quant Matrix Extension header :- \n" );
    report( severity_info, "    load_intra_quantizer_matrix  : %6d\n", Header->load_intra_quantizer_matrix );
    if( Header->load_intra_quantizer_matrix )
	for( i=0; i<64; i+=16 )
	{
	int j;
	    report( severity_info, "            " );
	    for( j=0; j<16; j++ )
		report( severity_info, "%02x ", Header->intra_quantizer_matrix[i+j] );
	    report( severity_info, "\n" );
	}

    report( severity_info, "    load_non_intra_quantizer_matrix  : %6d\n", Header->load_non_intra_quantizer_matrix );
    if( Header->load_non_intra_quantizer_matrix )
	for( i=0; i<64; i+=16 )
	{
	int j;
	    report( severity_info, "            " );
	    for( j=0; j<16; j++ )
		report( severity_info, "%02x ", Header->non_intra_quantizer_matrix[i+j] );
	    report( severity_info, "\n" );
	}

    report( severity_info, "    load_chroma_intra_quantizer_matrix  : %6d\n", Header->load_chroma_intra_quantizer_matrix );
    if( Header->load_chroma_intra_quantizer_matrix )
	for( i=0; i<64; i+=16 )
	{
	int j;
	    report( severity_info, "            " );
	    for( j=0; j<16; j++ )
		report( severity_info, "%02x ", Header->chroma_intra_quantizer_matrix[i+j] );
	    report( severity_info, "\n" );
	}

    report( severity_info, "    load_chroma_non_intra_quantizer_matrix  : %6d\n", Header->load_chroma_non_intra_quantizer_matrix );
    if( Header->load_chroma_non_intra_quantizer_matrix )
	for( i=0; i<64; i+=16 )
	{
	int j;
	    report( severity_info, "            " );
	    for( j=0; j<16; j++ )
		report( severity_info, "%02x ", Header->chroma_non_intra_quantizer_matrix[i+j] );
	    report( severity_info, "\n" );
	}
#endif

    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in a picture display extension header
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::ReadPictureDisplayExtensionHeader( void )
{
unsigned int                             i;
unsigned int                             NumberOfOffsets;
Mpeg2VideoPictureDisplayExtension_t     *Header;
bool                                     ProgressiveSequence;
bool                                     Frame;
bool                                     RepeatFirstField;
bool                                     TopFieldFirst;

//

    if( (FrameParameters == NULL) || !FrameParameters->PictureHeaderPresent )
    {
	report( severity_error, "FrameParser_VideoMpeg2_c::ReadPictureDisplayExtensionHeader - Appropriate picture header not found.\n" );
	return FrameParserNoStreamParameters;
    }

//

    Header      = &FrameParameters->PictureDisplayExtensionHeader;
    memset( Header, 0x00, sizeof(Mpeg2VideoPictureDisplayExtension_t) );

//

    ProgressiveSequence = !StreamParameters->SequenceExtensionHeaderPresent ||
			  StreamParameters->SequenceExtensionHeader.progressive_sequence;

    Frame               = !FrameParameters->PictureCodingExtensionHeaderPresent ||
			  (FrameParameters->PictureCodingExtensionHeader.picture_structure == MPEG2_PICTURE_STRUCTURE_FRAME);

    RepeatFirstField    = FrameParameters->PictureCodingExtensionHeaderPresent &&
			  FrameParameters->PictureCodingExtensionHeader.repeat_first_field;

    TopFieldFirst       = FrameParameters->PictureCodingExtensionHeaderPresent &&
			  FrameParameters->PictureCodingExtensionHeader.top_field_first;

//

    if( !Legal(ProgressiveSequence, Frame, TopFieldFirst, RepeatFirstField) )
    {
	report( severity_error, "FrameParser_VideoMpeg2_c::ReadPictureDisplayExtensionHeader - Illegal combination of progressive_sequence, progressive_frame,  top_field_first and repeat_first_field(%c %c %c %c).\n",
		(ProgressiveSequence    ? 'T' : 'F'),
		(Frame                  ? 'T' : 'F'),
		(TopFieldFirst          ? 'T' : 'F'),
		(RepeatFirstField       ? 'T' : 'F') );

	return FrameParserHeaderSyntaxError;
    }

    NumberOfOffsets     = PanScanCount(ProgressiveSequence, Frame, TopFieldFirst, RepeatFirstField);

//

    for( i=0; i<NumberOfOffsets; i++ )
    {
	Header->frame_centre[i].horizontal_offset       = Bits.SignedGet(16);
	MarkerBit(1);
	Header->frame_centre[i].vertical_offset         = Bits.SignedGet(16);
	MarkerBit(1)
    }

//

    FrameParameters->PictureDisplayExtensionHeaderPresent       = true;

//

#ifdef DUMP_HEADERS
    report( severity_info, "Picture Display Extension header   :-\n" );
    for( i=0; i<NumberOfOffsets; i++ )
    {
	report( severity_info, "    frame_centre[%d].horizontal_offset       : %6d\n", i, Header->frame_centre[i].horizontal_offset );
	report( severity_info, "    frame_centre[%d].vertical_offset         : %6d\n", i, Header->frame_centre[i].vertical_offset );
    }
#endif

    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in a picture temporal scalable extension header
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::ReadPictureTemporalScalableExtensionHeader( void )
{
Mpeg2VideoPictureTemporalScalableExtension_t    *Header;

//

    if( (FrameParameters == NULL) || !FrameParameters->PictureHeaderPresent )
    {
	report( severity_error, "FrameParser_VideoMpeg2_c::ReadPictureTemporalScalableExtensionHeader - Appropriate picture header not found.\n" );
	return FrameParserNoStreamParameters;
    }

    Header      = &FrameParameters->PictureTemporalScalableExtensionHeader;
    memset( Header, 0x00, sizeof(Mpeg2VideoPictureTemporalScalableExtension_t) );

//

    Header->reference_select_code               = Bits.Get(2);
    Header->forward_temporal_reference          = Bits.Get(10);
    MarkerBit(1);
    Header->backward_temporal_reference         = Bits.Get(10);

//

    FrameParameters->PictureTemporalScalableExtensionHeaderPresent      = true;

//

#ifdef DUMP_HEADERS
    report( severity_info, "Picture Temporal Scalable Extension header   :-\n" );
    report( severity_info, "    reference_select_code                    : %6d\n", Header->reference_select_code );
    report( severity_info, "    forward_temporal_reference               : %6d\n", Header->forward_temporal_reference );
    report( severity_info, "    backward_temporal_reference              : %6d\n", Header->backward_temporal_reference );
#endif

    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in a picture spatial scalable extension header
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::ReadPictureSpatialScalableExtensionHeader( void )
{
Mpeg2VideoPictureSpatialScalableExtension_t     *Header;

//

    if( (FrameParameters == NULL) || !FrameParameters->PictureHeaderPresent )
    {
	report( severity_error, "FrameParser_VideoMpeg2_c::ReadPictureSpatialScalableExtensionHeader - Appropriate picture header not found.\n" );
	return FrameParserNoStreamParameters;
    }

    Header      = &FrameParameters->PictureSpatialScalableExtensionHeader;
    memset( Header, 0x00, sizeof(Mpeg2VideoPictureSpatialScalableExtension_t) );

//

    Header->lower_layer_temporal_reference              = Bits.Get(10);
    MarkerBit(1);
    Header->lower_layer_horizontal_offset               = Bits.Get(15);
    MarkerBit(1);
    Header->lower_layer_vertical_offset                 = Bits.Get(15);
    Header->spatial_temporal_weight_code_table_index    = Bits.Get(2);
    Header->lower_layer_progressive_frame               = Bits.Get(1);
    Header->lower_layer_deinterlaced_field_select       = Bits.Get(1);

//

    FrameParameters->PictureSpatialScalableExtensionHeaderPresent       = true;

//

#ifdef DUMP_HEADERS
    report( severity_info, "Picture Spatial Scalable Extension header   :-\n" );
    report( severity_info, "    lower_layer_temporal_reference          : %6d\n", Header->lower_layer_temporal_reference );
    report( severity_info, "    lower_layer_horizontal_offset           : %6d\n", Header->lower_layer_horizontal_offset );
    report( severity_info, "    lower_layer_vertical_offset             : %6d\n", Header->lower_layer_vertical_offset );
    report( severity_info, "    spatial_temporal_weight_code_table_index: %6d\n", Header->spatial_temporal_weight_code_table_index );
    report( severity_info, "    lower_layer_progressive_frame           : %6d\n", Header->lower_layer_progressive_frame );
    report( severity_info, "    lower_layer_deinterlaced_field_select   : %6d\n", Header->lower_layer_deinterlaced_field_select );
#endif

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      On a first slice code, we should have garnered all the data 
//      we require we for a frame decode, this function records that fact.
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::CommitFrameForDecode( void )
{
unsigned int             i;
bool                     ProgressiveSequence;
bool                     FieldSequenceError;
PictureStructure_t       PictureStructure;
bool                     Frame;
bool                     RepeatFirstField;
bool                     TopFieldFirst;
unsigned int             PanAndScanCount;
unsigned int             Height, Width;
unsigned char            Policy;
SliceType_t              SliceType;
MatrixCoefficientsType_t MatrixCoefficients;

    //
    // Check we have the headers we need
    //

    if( (StreamParameters == NULL) ||
	!StreamParameters->SequenceHeaderPresent ||
	((StreamParameters->StreamType == MpegStreamTypeMpeg2) && !StreamParameters->SequenceExtensionHeaderPresent) )
    {
	report( severity_error, "FrameParser_VideoMpeg2_c::CommitFrameForDecode - Stream parameters unavailable for decode.\n" );
	return FrameParserNoStreamParameters;
    }

    if( (FrameParameters == NULL) ||
	!FrameParameters->PictureHeaderPresent ||
	((StreamParameters->StreamType == MpegStreamTypeMpeg2) && !FrameParameters->PictureCodingExtensionHeaderPresent) )
    {
	report( severity_error, "FrameParser_VideoMpeg2_c::CommitFrameForDecode - Frame parameters unavailable for decode.\n" );
	report( severity_info, "        %d %d %d\n", 
		(FrameParameters == NULL),
		!FrameParameters->PictureHeaderPresent,
		((StreamParameters->StreamType == MpegStreamTypeMpeg2) && !FrameParameters->PictureCodingExtensionHeaderPresent) );
	Player->MarkStreamUnPlayable( Stream );
	return FrameParserPartialFrameParameters;
    }

    //
    // Check that the aspect ratio code is valid for the specific standard
    //

    if( (StreamParameters->StreamType == MpegStreamTypeMpeg2) ?
		!inrange(StreamParameters->SequenceHeader.aspect_ratio_information, MIN_LEGAL_MPEG2_ASPECT_RATIO_CODE, MAX_LEGAL_MPEG2_ASPECT_RATIO_CODE) :
		!inrange(StreamParameters->SequenceHeader.aspect_ratio_information, MIN_LEGAL_MPEG1_ASPECT_RATIO_CODE, MAX_LEGAL_MPEG1_ASPECT_RATIO_CODE) )
    {
	report( severity_error, "FrameParser_VideoMpeg2_c::CommitFrameForDecode - aspect_ratio_information has illegal value (%02x) for mpeg standard.\n", StreamParameters->SequenceHeader.aspect_ratio_information );
	return FrameParserHeaderSyntaxError;
    }

    //
    // Check that the constrained parameters flag is valid for the specific standard
    //

    if( (StreamParameters->StreamType == MpegStreamTypeMpeg2) && 
	(StreamParameters->SequenceHeader.constrained_parameters_flag != 0) )
    {
	report( severity_error, "FrameParser_VideoMpeg2_c::CommitFrameForDecode - constrained_parameters_flag has illegal value (%02x) for mpeg2 standard.\n", StreamParameters->SequenceHeader.constrained_parameters_flag );
	return FrameParserHeaderSyntaxError;
    }

    //
    // Obtain and check the progressive etc values.
    //

    ProgressiveSequence = !StreamParameters->SequenceExtensionHeaderPresent ||
			  StreamParameters->SequenceExtensionHeader.progressive_sequence;

    PictureStructure    = FrameParameters->PictureCodingExtensionHeaderPresent ? 
				PictureStructures[FrameParameters->PictureCodingExtensionHeader.picture_structure] :
				StructureFrame;

    SliceType           = SliceTypeTranslation[FrameParameters->PictureHeader.picture_coding_type];
    Frame               = PictureStructure == StructureFrame;

    RepeatFirstField    = FrameParameters->PictureCodingExtensionHeaderPresent &&
			  FrameParameters->PictureCodingExtensionHeader.repeat_first_field;

    TopFieldFirst       = FrameParameters->PictureCodingExtensionHeaderPresent &&
			  FrameParameters->PictureCodingExtensionHeader.top_field_first;

    PanAndScanCount     = FrameParameters->PictureDisplayExtensionHeaderPresent ? 
				PanScanCount(ProgressiveSequence, Frame, TopFieldFirst, RepeatFirstField) :
				0;

//

    if( !Legal(ProgressiveSequence, Frame, TopFieldFirst, RepeatFirstField) )
    {
	report( severity_error, "FrameParser_VideoMpeg2_c::CommitFrameForDecode - Illegal combination of progressive_sequence, progressive_frame, repeat_first_field and top_field_first (%c %c %c %c).\n",
		(ProgressiveSequence    ? 'T' : 'F'),
		(Frame                  ? 'T' : 'F'),
		(RepeatFirstField       ? 'T' : 'F'),
		(TopFieldFirst          ? 'T' : 'F') );

	return FrameParserHeaderSyntaxError;
    }

    //
    // If we are doing field decode check for sequence error, and set appropriate flags
    // Update AccumulatedPictureStructure for nex pass, if this is the first decode of a field picture 
    // it is what we have otherwise it is empty
    //

    FieldSequenceError          = (AccumulatedPictureStructure == PictureStructure);
    FirstDecodeOfFrame          = FieldSequenceError || (AccumulatedPictureStructure == StructureEmpty);
    AccumulatedPictureStructure = (FirstDecodeOfFrame && !Frame) ? PictureStructure : StructureEmpty;

    if( FieldSequenceError )
    {
	report( severity_error, "FrameParser_VideoMpeg2_c::CommitFrameForDecode - Field sequence error detected.\n" );

	Player->CallInSequence( Stream, SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnOutputPartialDecodeBuffers );
    }

    //
    // For a field decode we need to recalculate top field first, 
    // as this is always set false for a field picture.
    //

    if( !Frame )
    {
	TopFieldFirst   = (FirstDecodeOfFrame == (PictureStructure == StructureTopField));
    }

    //
    // Deduce the matrix coefficients for colour conversions
    //

    if( StreamParameters->StreamType == MpegStreamTypeMpeg1 )
    {
	MatrixCoefficients      = MatrixCoefficients_ITU_R_BT601;
    }
    else if( StreamParameters->SequenceDisplayExtensionHeaderPresent &&
	     StreamParameters->SequenceDisplayExtensionHeader.color_description )
    {
	switch( StreamParameters->SequenceDisplayExtensionHeader.matrix_coefficients )
	{
	    case MPEG2_MATRIX_COEFFICIENTS_BT709:       MatrixCoefficients      = MatrixCoefficients_ITU_R_BT709;       break;
	    case MPEG2_MATRIX_COEFFICIENTS_FCC:         MatrixCoefficients      = MatrixCoefficients_FCC;               break;
	    case MPEG2_MATRIX_COEFFICIENTS_BT470_BGI:   MatrixCoefficients      = MatrixCoefficients_ITU_R_BT470_2_BG;  break;
	    case MPEG2_MATRIX_COEFFICIENTS_SMPTE_170M:  MatrixCoefficients      = MatrixCoefficients_SMPTE_170M;        break;
	    case MPEG2_MATRIX_COEFFICIENTS_SMPTE_240M:  MatrixCoefficients      = MatrixCoefficients_SMPTE_240M;        break;

	    default:
	    case MPEG2_MATRIX_COEFFICIENTS_FORBIDDEN:
	    case MPEG2_MATRIX_COEFFICIENTS_RESERVED:
							report( severity_error, "FrameParser_VideoMpeg2_c::CommitFrameForDecode - Forbidden or reserved matrix coefficient code specified (%02x)\n", StreamParameters->SequenceDisplayExtensionHeader.matrix_coefficients );
							// fall through

	    case MPEG2_MATRIX_COEFFICIENTS_UNSPECIFIED: MatrixCoefficients      = MatrixCoefficients_Undefined;         break;
	}
    }
    else
    {
	Policy      = Player->PolicyValue( Playback, Stream, PolicyMPEG2ApplicationType );
	switch( Policy )
	{
	    case PolicyValueMPEG2ApplicationMpeg2:
	    case PolicyValueMPEG2ApplicationAtsc:       MatrixCoefficients      = MatrixCoefficients_ITU_R_BT709;
							break;

	    default:
	    case PolicyValueMPEG2ApplicationDvb:        if( StreamParameters->SequenceHeader.horizontal_size_value > 720 )
							    MatrixCoefficients  = MatrixCoefficients_ITU_R_BT709;
							else if( FrameRates(StreamParameters->SequenceHeader.frame_rate_code) < 28 )
							    MatrixCoefficients  = MatrixCoefficients_ITU_R_BT470_2_BG;
							else
							    MatrixCoefficients  = MatrixCoefficients_SMPTE_170M;

							break;
	}
    }

    //
    // Record the stream and frame parameters into the appropriate structure
    //

    ParsedFrameParameters->FirstParsedParametersForOutputFrame          = FirstDecodeOfFrame;
    ParsedFrameParameters->FirstParsedParametersAfterInputJump          = FirstDecodeAfterInputJump;
    ParsedFrameParameters->SurplusDataInjected                          = SurplusDataInjected;
    ParsedFrameParameters->ContinuousReverseJump                        = ContinuousReverseJump;
    ParsedFrameParameters->KeyFrame                                     = SliceType == SliceTypeI;
    ParsedFrameParameters->ReferenceFrame                               = SliceType != SliceTypeB;
    ParsedFrameParameters->IndependentFrame                             = ParsedFrameParameters->KeyFrame;

    ParsedFrameParameters->NewStreamParameters                          = NewStreamParametersCheck();
    ParsedFrameParameters->SizeofStreamParameterStructure               = sizeof(Mpeg2StreamParameters_t);
    ParsedFrameParameters->StreamParameterStructure                     = StreamParameters;

    ParsedFrameParameters->NewFrameParameters                           = true;
    ParsedFrameParameters->SizeofFrameParameterStructure                = sizeof(Mpeg2FrameParameters_t);
    ParsedFrameParameters->FrameParameterStructure                      = FrameParameters;

//

    ParsedVideoParameters->Content.Width                                = StreamParameters->SequenceHeader.horizontal_size_value;
    ParsedVideoParameters->Content.Height                               = StreamParameters->SequenceHeader.vertical_size_value;
    ParsedVideoParameters->Content.DisplayWidth                         = ParsedVideoParameters->Content.Width;
    ParsedVideoParameters->Content.DisplayHeight                        = ParsedVideoParameters->Content.Height;
    if( StreamParameters->SequenceDisplayExtensionHeaderPresent )
    {
	ParsedVideoParameters->Content.DisplayWidth                     = StreamParameters->SequenceDisplayExtensionHeader.display_horizontal_size;
	ParsedVideoParameters->Content.DisplayHeight                    = StreamParameters->SequenceDisplayExtensionHeader.display_vertical_size;
    }

    ParsedVideoParameters->Content.Progressive                          = ProgressiveSequence;
    ParsedVideoParameters->Content.OverscanAppropriate                  = false;
    ParsedVideoParameters->Content.PixelAspectRatio                     = (StreamParameters->StreamType == MpegStreamTypeMpeg2) ? 
										Mpeg2AspectRatios(StreamParameters->SequenceHeader.aspect_ratio_information) :
										Mpeg1AspectRatios(StreamParameters->SequenceHeader.aspect_ratio_information);
    ParsedVideoParameters->Content.FrameRate                            = FrameRates(StreamParameters->SequenceHeader.frame_rate_code);

    ParsedVideoParameters->Content.VideoFullRange                       = false;
    ParsedVideoParameters->Content.ColourMatrixCoefficients             = MatrixCoefficients;

    ParsedVideoParameters->SliceType                                    = SliceType;
    ParsedVideoParameters->PictureStructure                             = PictureStructure;
    ParsedVideoParameters->InterlacedFrame                              = FrameParameters->PictureCodingExtensionHeaderPresent ? (FrameParameters->PictureCodingExtensionHeader.progressive_frame == 0) : false;
    ParsedVideoParameters->TopFieldFirst                                = !FrameParameters->PictureCodingExtensionHeaderPresent || TopFieldFirst;

    ParsedVideoParameters->DisplayCount[0]                              = DisplayCount0(ProgressiveSequence, Frame, TopFieldFirst, RepeatFirstField);
    ParsedVideoParameters->DisplayCount[1]                              = DisplayCount1(ProgressiveSequence, Frame, TopFieldFirst, RepeatFirstField);

    //
    // Specialist code for broadcast streams that do not get progressive_frame right
    //
    // Whay we do is treat every frame as interlaced, except those which we have 
    // good reason not to. If for example we have seen a repeat first field in the 
    // stream then we suspect that some sort of pulldown (possibly 3:2) may be in effect.
    //

    Policy      = Player->PolicyValue( Playback, Stream, PolicyMPEG2DoNotHonourProgressiveFrameFlag );
    if( Policy == PolicyValueApply )
    {
	if( RepeatFirstField )
	    EverSeenRepeatFirstField                                    = true;

	if( !EverSeenRepeatFirstField )
	    ParsedVideoParameters->InterlacedFrame                      = !ProgressiveSequence;
	else
	    ParsedVideoParameters->InterlacedFrame                      = false;
    }

    //
    // Now insert the pan and scan counts if we have them, 
    // alternatively repeat the last value for the appropriate period
    //

    if( PanAndScanCount != 0 )
    {
	ParsedVideoParameters->PanScan.Count                            = PanAndScanCount;
	for( i=0; i<PanAndScanCount; i++ )
	{
	    ParsedVideoParameters->PanScan.DisplayCount[i]              = 1;
	    ParsedVideoParameters->PanScan.HorizontalOffset[i]          = FrameParameters->PictureDisplayExtensionHeader.frame_centre[i].horizontal_offset;
	    ParsedVideoParameters->PanScan.VerticalOffset[i]            = FrameParameters->PictureDisplayExtensionHeader.frame_centre[i].vertical_offset;
	}

	LastPanScanHorizontalOffset                                     = ParsedVideoParameters->PanScan.HorizontalOffset[PanAndScanCount-1];
	LastPanScanVerticalOffset                                       = ParsedVideoParameters->PanScan.VerticalOffset[PanAndScanCount-1];
    }
    else
    {
	ParsedVideoParameters->PanScan.Count                            = 1;
	ParsedVideoParameters->PanScan.DisplayCount[0]                  = ParsedVideoParameters->DisplayCount[0] + ParsedVideoParameters->DisplayCount[1];
	ParsedVideoParameters->PanScan.HorizontalOffset[0]              = LastPanScanHorizontalOffset;
	ParsedVideoParameters->PanScan.VerticalOffset[0]                = LastPanScanVerticalOffset;
    }

    //
    // If we have a Display aspect ratio, convert to a sample aspect ratio
    //

    if( (ParsedVideoParameters->Content.PixelAspectRatio != Rational_t(1,1)) &&
	(StreamParameters->StreamType == MpegStreamTypeMpeg2) )
    {
	Width                                   = ParsedVideoParameters->Content.DisplayWidth;
	Height                                  = ParsedVideoParameters->Content.DisplayHeight;
	// This is a bodge to cope with streams which set their aspect ratios on the width/height
	// rather than DisplayWidth/DisplayHeight (e.g. Rome and Stargate).
	if ((ParsedVideoParameters->Content.DisplayWidth < ParsedVideoParameters->Content.Width) &&
	    (StreamParameters->SequenceHeader.aspect_ratio_information > 2))
	{
	    Width                               = ParsedVideoParameters->Content.Width;
	    Height                              = ParsedVideoParameters->Content.Height;
	}
	//ParsedVideoParameters->Content.PixelAspectRatio = (ParsedVideoParameters->Content.PixelAspectRatio * Width) / Height;
	// Convert to height/width similar to h264, vc1 etc
	ParsedVideoParameters->Content.PixelAspectRatio = (ParsedVideoParameters->Content.PixelAspectRatio * Height) / Width;
    }

    //
    // Record our claim on both the frame and stream parameters
    //

    Buffer->AttachBuffer( StreamParametersBuffer );
    Buffer->AttachBuffer( FrameParametersBuffer );

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

    FrameToDecode       = true;

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Boolean function to evaluate whether or not the stream 
//      parameters are new.
//

bool   FrameParser_VideoMpeg2_c::NewStreamParametersCheck( void )
{
bool            Different;

    //
    // The parameters cannot be new if they have been used before.
    //

    if( !StreamParameters->UpdatedSinceLastFrame )
	return false;

    StreamParameters->UpdatedSinceLastFrame     = false;

    //
    // Check for difference using a straightforward comparison to see if the
    // stream parameters have changed. (since we zero on allocation simple
    // memcmp should be sufficient).
    //

    Different   = memcmp( &CopyOfStreamParameters, StreamParameters, sizeof(Mpeg2StreamParameters_t) ) != 0;
    if( Different )
    {
	memcpy( &CopyOfStreamParameters, StreamParameters, sizeof(Mpeg2StreamParameters_t) );
	return true;
    }

//

    return false;
}
