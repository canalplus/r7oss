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

Source file name : frame_parser_video_h264.cpp
Author :           Nick

Implementation of the h264 video frame parser class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
27-Jul-07   Created                                         Nick

************************************************************************/

//#define DUMP_HEADERS
//#define DUMP_REFLISTS

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "frame_parser_video_h264.h"
#include "ring_generic.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

static BufferDataDescriptor_t     H264StreamParametersDescriptor = BUFFER_H264_STREAM_PARAMETERS_TYPE;
static BufferDataDescriptor_t     H264FrameParametersDescriptor = BUFFER_H264_FRAME_PARAMETERS_TYPE;

//

#define BUFFER_SEQUENCE_PARAMETER_SET		"H264SequenceParameterSet"
#define BUFFER_SEQUENCE_PARAMETER_SET_TYPE	{BUFFER_SEQUENCE_PARAMETER_SET, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(SequenceParameterSetPair_t)}

#define BUFFER_PICTURE_PARAMETER_SET		"H264PictureParameterSet"
#define BUFFER_PICTURE_PARAMETER_SET_TYPE	{BUFFER_PICTURE_PARAMETER_SET, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(H264PictureParameterSetHeader_t)}

static BufferDataDescriptor_t     SequenceParameterSetDescriptor = BUFFER_SEQUENCE_PARAMETER_SET_TYPE;
static BufferDataDescriptor_t     PictureParameterSetDescriptor = BUFFER_PICTURE_PARAMETER_SET_TYPE;

//

#define MIN_LEGAL_H264_ASPECT_RATIO_CODE	0
#define MAX_LEGAL_H264_ASPECT_RATIO_CODE	16

// BEWARE !!!! you cannot declare static initializers of a constructed type such as Rational_t
//             the compiler will silently ignore them..........
static unsigned int     H264AspectRatioValues[][2]     =
    {
        {  1,  1 },			// Not strictly specified
        {  1,  1 },
        { 12, 11 },
        { 10, 11 },
        { 16, 11 },
        { 40, 33 },
        { 24, 11 },
        { 20, 11 },
        { 32, 11 },
        { 80, 33 },
        { 18, 11 },
        { 15, 11 },
        { 64, 33 },
        {160, 99 }, 
        {  4,  3 }, 
        {  3,  2 }, 
        {  2,  1 } 
    };

#define H264AspectRatios(N) Rational_t(H264AspectRatioValues[N][0],H264AspectRatioValues[N][1])

//

static SliceType_t SliceTypeTranslation[]  = { SliceTypeP, SliceTypeB, SliceTypeI, SliceTypeP, SliceTypeI, SliceTypeP, SliceTypeB, SliceTypeI, SliceTypeP, SliceTypeI };

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined macros
//

#define u(n)                    Bits.Get(n)
#define ue(v)                   Bits.GetUe()
#define se(v)                   Bits.GetSe()

#define Log2Ceil(v)             (32 - __lzcntw(v))

#define Assert(L)		if( !(L) )											 	\
				{													\
				    report( severity_error, "Assertion fail %s %d\n", __FUNCTION__, __LINE__ );	\
				    /*Player->MarkStreamUnPlayable( Stream );*/								\
				    return FrameParserError;										\
				}

#define AssertAntiEmulationOk(Text)										\
				{										\
				FrameParserStatus_t	Status;							\
				    Status  = TestAntiEmulationBuffer();					\
				    if( Status != FrameParserNoError )						\
				    {										\
				        report( severity_error, "FrameParser_VideoH264_c::%s - Anti Emulation Test fail.\n", Text );	\
					/*Player->MarkStreamUnPlayable( Stream );*/					\
					return FrameParserError;						\
				    }										\
				}

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

// /////////////////////////////////////////////////////////////////////////
//
//      Constructor
//

FrameParser_VideoH264_c::FrameParser_VideoH264_c( void )
{
    Configuration.FrameParserName		= "VideoH264";

    Configuration.StreamParametersCount		= H264_STREAM_PARAMETERS_COUNT;
    Configuration.StreamParametersDescriptor	= &H264StreamParametersDescriptor;

    Configuration.FrameParametersCount		= H264_FRAME_PARAMETERS_COUNT;
    Configuration.FrameParametersDescriptor	= &H264FrameParametersDescriptor;

    Configuration.MaxReferenceFrameCount	= H264_MAX_REFERENCE_FRAMES;

//

    SequenceParameterSetPool		= NULL;
    PictureParameterSetPool		= NULL;

    PrepareScannedScalingLists();

    memset( SequenceParameterSetTable, 0x00, H264_STANDARD_MAX_SEQUENCE_PARAMETER_SETS * sizeof(SequenceParameterSetEntry_t) );
    memset( PictureParameterSetTable, 0x00, H264_STANDARD_MAX_PICTURE_PARAMETER_SETS * sizeof(PictureParameterSetEntry_t) );

    Reset();
}

// /////////////////////////////////////////////////////////////////////////
//
//      Destructor
//

FrameParser_VideoH264_c::~FrameParser_VideoH264_c( void )
{
    Halt();
    Reset();
}

// /////////////////////////////////////////////////////////////////////////
//
//      The Reset function release any resources, and reset all variable
//

FrameParserStatus_t   FrameParser_VideoH264_c::Reset(  void )
{
unsigned int	i;

//

    Configuration.SupportSmoothReversePlay	= true;

//

    memset( &CopyOfSequenceParameterSet,          0x00, sizeof(H264SequenceParameterSetHeader_t) );
    memset( &CopyOfSequenceParameterSetExtension, 0x00, sizeof(H264SequenceParameterSetExtensionHeader_t) );
    memset( &CopyOfPictureParameterSet,           0x00, sizeof(H264PictureParameterSetHeader_t) );

    memset( &SEIPictureTiming,			  0x00, sizeof(H264SEIPictureTiming_t) );
    memset( &SEIPanScanRectangle,		  0x00, sizeof(H264SEIPanScanRectangle_t) );

    memset( &PanScanState, 			  0x00, sizeof(PanScanState_t) );

    memset( &ReferenceFrameList, 0x00, H264_NUM_REF_FRAME_LISTS * sizeof(ReferenceFrameList_t) );
    memset( &ReferenceFrames, 0x00,    (H264_MAX_REFERENCE_FRAMES + 1) * sizeof(H264ReferenceFrameData_t) );

    memset( DeferredList, 0x00, H264_CODED_FRAME_COUNT * sizeof(H264DeferredDFIandPTSList_t) );
    memset( OrderedDeferredList, 0x00, H264_CODED_FRAME_COUNT * sizeof(unsigned int) );

    StreamParameters                            = NULL;
    FrameParameters                             = NULL;
    DeferredParsedFrameParameters               = NULL;
    DeferredParsedVideoParameters               = NULL;

    UserSpecifiedDefaultFrameRate		= false;
    DefaultFrameRate				= 24;
    DefaultPixelAspectRatio			= 1;

    ReadNewSPS					= false;
    ReadNewSPSExtension				= false;
    ReadNewPPS					= false;
    SeenAnIDR					= false;
    BehaveAsIfSeenAnIDR				= false;
    FirstDecodeOfFrame                          = true;
    AccumulatedFrameNumber			= INVALID_INDEX;

    DeferredListEntries				= 0;

    FirstFieldSeen				= false;
    FixDeducedFlags				= false;
    DeducedInterlacedFlag			= false;
    DeducedTopFieldFirst			= false;
    ForceInterlacedProgressive			= false;

    FrameSliceType				= H264_SLICE_TYPE_I;
    FrameSliceTypeVaries			= false;

    SeenProbableReversalPoint			= false;

    //
    // Reset H264 context variables
    //

    NumReferenceFrames		= 0;
    NumLongTerm			= 0;
    NumShortTerm		= 0;

    CpbDpbDelaysPresentFlag     = 0;
    LastCpbDpbDelaysPresentFlag	= 2;				// An illegal value for a flag
    CpbRemovalDelayLength       = 0;
    DpbOutputDelayLength        = 0;
    PicStructPresentFlag        = 0;
    TimeOffsetLength            = 0;

    PrevPicOrderCntMsb          = 0;
    PrevPicOrderCntLsb          = 0;
    PrevFrameNum		= 0;
    PrevFrameNumOffset		= 0;
    PicOrderCntOffset		= 0x100000000ULL;		// Guarantee the extended value never goes negative
    BaseDpbValue		= 0;
    SeenDpbValue		= false;

    DisplayOrderByDpbValues	= false;
    DpbValuesInvalidatedByPTS	= false;

    //
    // We now reset the lower level, before resetting the two parameter set pools
    // this is because stream parameters (which are freed in the lower level reset)
    // may have parameter sets attached to them.
    //

    FrameParser_Video_c::Reset();

    //
    // Destroy the paramneter set pools - picture parameter set first, 
    // since it may have attached sequenece parameter sets.
    //

    if( PictureParameterSetPool != NULL )
    {
	for( i=0; i<H264_STANDARD_MAX_PICTURE_PARAMETER_SETS; i++ )
	    if( PictureParameterSetTable[i].Buffer != NULL )
	    {
		PictureParameterSetTable[i].Buffer->DecrementReferenceCount();
		PictureParameterSetTable[i].Buffer	= NULL;
	    }

	BufferManager->DestroyPool( PictureParameterSetPool );
	PictureParameterSetPool		= NULL;
    }

//

    if( SequenceParameterSetPool != NULL )
    {
	for( i=0; i<H264_STANDARD_MAX_SEQUENCE_PARAMETER_SETS; i++ )
	    if( SequenceParameterSetTable[i].Buffer != NULL )
	    {
		SequenceParameterSetTable[i].Buffer->DecrementReferenceCount();
		SequenceParameterSetTable[i].Buffer	= NULL;
	    }

	BufferManager->DestroyPool( SequenceParameterSetPool );
	SequenceParameterSetPool	= NULL;
    }

//

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The register output ring function
//

FrameParserStatus_t   FrameParser_VideoH264_c::RegisterOutputBufferRing(       Ring_t          Ring )
{
PlayerStatus_t  	 Status;

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
    // Attempt to register the types for sequence and picture parameter sets
    //

    Status      = BufferManager->FindBufferDataType( SequenceParameterSetDescriptor.TypeName, &SequenceParameterSetType );
    if( Status != BufferNoError )
    {
	Status  = BufferManager->CreateBufferDataType( &SequenceParameterSetDescriptor, &SequenceParameterSetType );
	if( Status != BufferNoError )
	{
	    report( severity_error, "FrameParser_VideoH264_c::RegisterOutputBufferRing - Failed to create the sequence parameter set buffer type.\n" );
	    return FrameParserError;
	}
    }

//

    Status      = BufferManager->FindBufferDataType( PictureParameterSetDescriptor.TypeName, &PictureParameterSetType );
    if( Status != BufferNoError )
    {
	Status  = BufferManager->CreateBufferDataType( &PictureParameterSetDescriptor, &PictureParameterSetType );
	if( Status != BufferNoError )
	{
	    report( severity_error, "FrameParser_VideoH264_c::RegisterOutputBufferRing - Failed to create the picture parameter set buffer type.\n" );
	    return FrameParserError;
	}
    }

    //
    // Create the pools of sequence and picture parameter sets
    //

    Status      = BufferManager->CreatePool( &SequenceParameterSetPool, SequenceParameterSetType, (H264_MAX_SEQUENCE_PARAMETER_SETS+H264_MAX_REFERENCE_FRAMES+1) );
    if( Status != BufferNoError )
    {
	report( severity_error, "FrameParser_VideoH264_c::RegisterOutputBufferRing - Failed to create a pool of sequence parameter sets.\n" );
	SetComponentState(ComponentInError);
	return Status;
    }

//

    Status      = BufferManager->CreatePool( &PictureParameterSetPool, PictureParameterSetType, (H264_MAX_PICTURE_PARAMETER_SETS+H264_MAX_REFERENCE_FRAMES+1) );
    if( Status != BufferNoError )
    {
	report( severity_error, "FrameParser_VideoH264_c::RegisterOutputBufferRing - Failed to create a pool of picture parameter sets.\n" );
	SetComponentState(ComponentInError);
	return Status;
    }

//

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The three helper functions for the collator, to allow it to 
//	determine which headers form frame boundaries.
//

FrameParserStatus_t   FrameParser_VideoH264_c::ResetCollatedHeaderState( void )
{
    // Until the reversible collator, there is no state to reset.

    SeenProbableReversalPoint	= false;
    return FrameParserNoError;
}

// -----------------------

unsigned int   FrameParser_VideoH264_c::RequiredPresentationLength(	unsigned char		  StartCode )
{
unsigned char	NalUnitType	= (StartCode & 0x1f);
unsigned int	ExtraBytes	= 0;

    if( (NalUnitType == NALU_TYPE_IDR) ||
	(NalUnitType == NALU_TYPE_AUX_SLICE) ||
        (NalUnitType == NALU_TYPE_SLICE) )
	ExtraBytes	= 1;

    return ExtraBytes;
}

// -----------------------

FrameParserStatus_t   FrameParser_VideoH264_c::PresentCollatedHeader(
							unsigned char		  StartCode,
							unsigned char		 *HeaderBytes,
							FrameParserHeaderFlag_t	 *Flags )
{
unsigned char	NalUnitType	= (StartCode & 0x1f);
bool		AllowIResync;
bool		Slice;
bool		FirstSlice;
bool		Iframe;

//

    *Flags	= 0;

//

    AllowIResync	= (Player->PolicyValue( Playback, Stream, PolicyH264AllowNonIDRResynchronization ) == PolicyValueApply);

    Slice		= (NalUnitType == NALU_TYPE_IDR) || (NalUnitType == NALU_TYPE_AUX_SLICE) || (NalUnitType == NALU_TYPE_SLICE);
    FirstSlice		= Slice && ((HeaderBytes[0] & 0x80) != 0);
    Iframe		= Slice && ((HeaderBytes[0] == 0x88) || ((HeaderBytes[0] & 0xf0) == 0xB0));	// H264_SLICE_TYPE_I_ALL or H264_SLICE_TYPE_I

//

    if( SeenProbableReversalPoint && (Slice || (NalUnitType == NALU_TYPE_SPS)) )
    {
	*Flags				|= FrameParserHeaderFlagConfirmReversiblePoint;

	if( NalUnitType == NALU_TYPE_SPS )
	    *Flags			|= FrameParserHeaderFlagPossibleReversiblePoint | FrameParserHeaderFlagPartitionPoint;		// Make this the reversible point

	SeenProbableReversalPoint	 = false;
    }

//

    if( (NalUnitType == NALU_TYPE_IDR) || (AllowIResync && Iframe) )
    {
	*Flags				|= FrameParserHeaderFlagPossibleReversiblePoint;
	SeenProbableReversalPoint	 = true;
    }

//

    if( FirstSlice )
	*Flags	|= FrameParserHeaderFlagPartitionPoint;

//

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The read headers stream specific function
//

FrameParserStatus_t   FrameParser_VideoH264_c::ReadHeaders( void )
{
unsigned int            i;
unsigned int		Code;
FrameParserStatus_t     Status;
bool                    IgnoreStreamComponents;
bool                    IgnoreFrameComponents;
unsigned char		Policy;
unsigned int		UnitLength;

//

    IgnoreFrameComponents               = true;
    IgnoreStreamComponents              = true;

//

    if( FirstDecodeAfterInputJump )
    {
	SeenAnIDR			= false;
	BehaveAsIfSeenAnIDR		= false;

	//
	// Ensure pic order cnt stuff will work for whatever direction we are playing in
	//

	if( PlaybackDirection == PlayBackward )
    	{
	    if( inrange(PicOrderCntOffset,0, 0x7fffffffffffffffull) )
	    	PicOrderCntOffset	= 0xffffffff00000000ull;

	    PicOrderCntOffsetAdjust	= 0xffffffff00000000ull;
    	}
    	else
    	{
	    if( !inrange(PicOrderCntOffset,0, 0x7fffffffffffffffull) )
	    	PicOrderCntOffset	= 0x100000000ull;

	    PicOrderCntOffsetAdjust	= 0x100000000ull;
    	}
    }

//

    for( i=0; i<StartCodeList->NumberOfStartCodes; i++ )
    {
	Code		= StartCodeList->StartCodes[i];
	UnitLength	= ((i != StartCodeList->NumberOfStartCodes-1) ? ExtractStartCodeOffset(StartCodeList->StartCodes[i+1]) : BufferLength) - ExtractStartCodeOffset(Code) - 4;

	if( (ExtractStartCodeCode(Code) & 0x80) != 0 )
	{
	    report( severity_error, "%s(%d) - Invalid marker bit value.\n", __FUNCTION__, __LINE__ );
	    return FrameParserHeaderSyntaxError;
        }

	nal_ref_idc		= ExtractStartCodeCode(Code) >> 5;
	nal_unit_type		= ExtractStartCodeCode(Code) & 0x1f;

//

#ifdef DUMP_HEADERS
	switch( nal_unit_type )
	{
	    case NALU_TYPE_SLICE:           report( severity_info, "\nNALU_TYPE_SLICE\n" );         break;
	    case NALU_TYPE_DPA:             report( severity_info, "\nNALU_TYPE_DPA\n" );           break;
	    case NALU_TYPE_DPB:             report( severity_info, "\nNALU_TYPE_DPB\n" );           break;
	    case NALU_TYPE_DPC:             report( severity_info, "\nNALU_TYPE_DPC\n" );           break;
	    case NALU_TYPE_IDR:             report( severity_info, "\nNALU_TYPE_IDR\n" );           break;
	    case NALU_TYPE_SEI:             report( severity_info, "\nNALU_TYPE_SEI\n" );           break;
	    case NALU_TYPE_SPS:             report( severity_info, "\nNALU_TYPE_SPS\n" );           break;
	    case NALU_TYPE_PPS:             report( severity_info, "\nNALU_TYPE_PPS\n" );           break;
	    case NALU_TYPE_PD:              report( severity_info, "\nNALU_TYPE_PD\n" );            break;
	    case NALU_TYPE_EOSEQ:           report( severity_info, "\nNALU_TYPE_EOSEQ\n" );         break;
	    case NALU_TYPE_EOSTREAM:        report( severity_info, "\nNALU_TYPE_EOSTREAM\n" );      break;
	    case NALU_TYPE_FILL:            report( severity_info, "\nNALU_TYPE_FILL\n" );          break;
	    case NALU_TYPE_SPS_EXT:         report( severity_info, "\nNALU_TYPE_SPS_EXT\n" );       break;
	    case NALU_TYPE_AUX_SLICE:       report( severity_info, "\nNALU_TYPE_AUX_SLICE\n" );     break;

	    case NALU_TYPE_PLAYER2_CONTAINER_PARAMETERS:       report( severity_info, "\nNALU_TYPE_PLAYER2_CONTAINER_PARAMETERS\n" );     break;

	    default:                        report( severity_info, "\nUnexpected NAL unit type %02x\n", nal_unit_type );            break;
	}
	report( severity_info, "    nal_ref_idc        %d\n", nal_ref_idc );
#endif

	//
	// Process the header, with a specific preload of data into the anti emulation buffer, dependent on header type
	//

	LoadAntiEmulationBuffer( BufferData + ExtractStartCodeOffset(Code) + 4 );	// Ignore the actual code (00 00 01 <Marker + idc + type>)

	Status  = FrameParserNoError;
	switch( nal_unit_type )
	{
	    case NALU_TYPE_IDR:
			SeenAnIDR		= true;
			BehaveAsIfSeenAnIDR	= true;
	    case NALU_TYPE_AUX_SLICE:
	    case NALU_TYPE_SLICE:
			if( !BehaveAsIfSeenAnIDR )
			{
			    Policy	= Player->PolicyValue( Playback, Stream, PolicyH264AllowNonIDRResynchronization );
			    if( Policy != PolicyValueApply )
			    {
				IgnoreFrameComponents	= true;
				report( severity_error, "FrameParser_VideoH264_c::ReadHeaders - Not seen an IDR.\n" );
				break;
			    }
			}

			CheckAntiEmulationBuffer( DEFAULT_ANTI_EMULATION_REQUEST );

			Status 		= ReadNalSliceHeader();

			// Commit for decode if status is ok, and this is not the first slice 
			// (IE we haven't already got the frame to decode flag, and its a first slice)
			if( (Status == FrameParserNoError) && !FrameToDecode )
			{
			    ParsedFrameParameters->DataOffset	= ExtractStartCodeOffset(Code);
			    Status				= CommitFrameForDecode();

			    SEIPictureTiming.Valid		= 0;		// any picture timning applied only to this decode
			}
			break;

	    case NALU_TYPE_SEI:
			CheckAntiEmulationBuffer( min(DEFAULT_ANTI_EMULATION_REQUEST, UnitLength) );
			Status = ReadNalSupplementalEnhancementInformation( UnitLength );
			if( SeenDpbValue )
			{
			    unsigned char ForcePicOrderCntValue;

			    ForcePicOrderCntValue		= Player->PolicyValue( Playback, Stream, PolicyH264ForcePicOrderCntIgnoreDpbDisplayFrameOrdering );
			    DisplayOrderByDpbValues		= (ForcePicOrderCntValue != PolicyValueApply) && !DpbValuesInvalidatedByPTS;
			}
			break;


	    case NALU_TYPE_SPS:
			CheckAntiEmulationBuffer( UnitLength );
			Status = ReadNalSequenceParameterSet();
			IgnoreStreamComponents  = (Status != FrameParserNoError);
			ReadNewSPS		= true;
			break;

	    case NALU_TYPE_SPS_EXT:
			if( IgnoreStreamComponents )
			    break;

			CheckAntiEmulationBuffer( UnitLength );
			Status = ReadNalSequenceParameterSetExtension();
			ReadNewSPSExtension	= true;
			break;

	    case NALU_TYPE_PPS:
			CheckAntiEmulationBuffer( UnitLength );
			Status = ReadNalPictureParameterSet();
			IgnoreFrameComponents  	= (Status != FrameParserNoError);
			ReadNewPPS		= true;
			break;

	    case NALU_TYPE_DPA:
	    case NALU_TYPE_DPB:
	    case NALU_TYPE_DPC:
	    case NALU_TYPE_PD:
	    case NALU_TYPE_EOSEQ:
	    case NALU_TYPE_EOSTREAM:
	    case NALU_TYPE_FILL:
			Status = FrameParserNoError;
			break;

	    case NALU_TYPE_PLAYER2_CONTAINER_PARAMETERS:
			CheckAntiEmulationBuffer( UnitLength );
			Status = ReadPlayer2ContainerParameters();
			break;

	    default:
			report( severity_info, "FrameParser_VideoH264_c::ReadHeaders - Ignored reserved NAL unit type %02x.\n", nal_unit_type );
			Status = FrameParserNoError;
			break;
	}

//

	AssertAntiEmulationOk( "ReadHeaders" );

	if( Status != FrameParserNoError )
	{
	    IgnoreStreamComponents      = true;
	    IgnoreFrameComponents       = true;
	}
    }

    //
    // We clear the FrameParameters pointer, a new one will be obtained
    // before/if we read in headers pertaining to the next frame. This 
    // will generate an error should I accidentally write code that 
    // accesses this data when it should not.
    //

    if( FrameToDecode )
	FrameParameters             = NULL;

    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Function to initialize an SPS header into a default state
//


FrameParserStatus_t   FrameParser_VideoH264_c::SetDefaultSequenceParameterSet( H264SequenceParameterSetHeader_t *Header )
{
unsigned int    i,j;

    //
    // Initialize affected context variables
    //

    CpbDpbDelaysPresentFlag                     = 0;
    CpbRemovalDelayLength                       = 0;
    DpbOutputDelayLength                        = 0;
    SeenDpbValue				= false;
    DisplayOrderByDpbValues			= false;

    PicStructPresentFlag                        = 0;
    TimeOffsetLength                            = 0;

    //
    // Default Chroma to 4:2:0 for not-high profile
    //

    Header->chroma_format_idc                   = 1;

    //
    // Set scaling list to Flat (default for MAIN and HIGH profile without scaling list)
    //

    for( i=0; i<6; i++ )
	for( j=0; j<16; j++ )
	    Header->ScalingList4x4[i][j]        = 16;

    for( i=0; i<2; i++ )
	for( j=0; j<64; j++ )
	    Header->ScalingList8x8[i][j]        = 16;

    //
    // Set colour primaries, transfer characteristics and matrix coefficients to unspecified
    //

    Header->vui_seq_parameters.video_full_range_flag            = 0;
    Header->vui_seq_parameters.colour_primaries                 = 2;
    Header->vui_seq_parameters.transfer_characteristics		= 2;
    Header->vui_seq_parameters.matrix_coefficients              = 2;

    //
    // Defaults to allow motion vectors over picture boundaries
    //

    Header->vui_seq_parameters.motion_vectors_over_pic_boundaries_flag  = 1;

    //
    // Default max fields
    //

    Header->vui_seq_parameters.max_bytes_per_pic_denom                  = 2;
    Header->vui_seq_parameters.max_bits_per_mb_denom                    = 1;

    Header->vui_seq_parameters.log2_max_mv_length_horizontal            = 16;
    Header->vui_seq_parameters.log2_max_mv_length_vertical              = 16;

//

    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Function to initialize a slice header into a default state (it is cheaper than clearing it)
//

FrameParserStatus_t   FrameParser_VideoH264_c::SetDefaultSliceHeader( H264SliceHeader_t *Header )
{
    Header->Valid 				= false;

    Header->PictureParameterSet			= NULL;
    Header->SequenceParameterSet		= NULL;
    Header->SequenceParameterSetExtension	= NULL;

    Header->field_pic_flag			= 0;
    Header->bottom_field_flag			= 0;
    Header->num_ref_idx_active_override_flag	= 0;
    Header->num_ref_idx_l0_active_minus1	= 0;
    Header->num_ref_idx_l1_active_minus1	= 0;
    Header->sp_for_switch_flag			= 0;
    Header->slice_qs_delta			= 0;
    Header->disable_deblocking_filter_idc	= 0;

    Header->ref_pic_list_reordering.ref_pic_list_reordering_flag_l0	= 0;
    Header->ref_pic_list_reordering.ref_pic_list_reordering_flag_l1	= 0;

    Header->dec_ref_pic_marking.no_ouptput_of_prior_pics_flag		= 0;
    Header->dec_ref_pic_marking.long_term_reference_flag		= 0;
    Header->dec_ref_pic_marking.adaptive_ref_pic_marking_mode_flag	= 0;
    Header->dec_ref_pic_marking.memory_management_control_operation[0]	= H264_MMC_END;

    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in a scaling list
//
///     Incorporating the Zscan ordering, and the default scaling lists
//

static unsigned char ZZScan4x4[16] =
{
    0,  1,  4,  8,  5,  2,  3,  6,  9, 12, 13, 10,  7, 11, 14, 15
};

static unsigned char ZZScan8x8[64] =
{
    0,  1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11,  4,  5,
   12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28,
   35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
   58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63
};

// -----------------------------------------

static unsigned int DefaultScalingList4x4Intra[16] = {   6, 13, 13, 20,
							20, 20, 28, 28,
							28, 28, 32, 32,
							32, 37, 37, 42};
static unsigned int DefaultScalingList4x4Inter[16] = {  10, 14, 14, 20,
							20, 20, 24, 24,
							24, 24, 27, 27,
							27, 30, 30, 34};
static unsigned int DefaultScalingList8x8Intra[64] = {   6, 10, 10, 13, 11, 13, 16, 16,
							16, 16, 18, 18, 18, 18, 18, 23,
							23, 23, 23, 23, 23, 25, 25, 25,
							25, 25, 25, 25, 27, 27, 27, 27,
							27, 27, 27, 27, 29, 29 ,29, 29,
							29, 29, 29, 31, 31, 31, 31, 31,
							31, 33, 33, 33, 33, 33, 36, 36,
							36, 36, 38, 38, 38, 40, 40, 42};
static unsigned int DefaultScalingList8x8Inter[64] = {   9, 13, 13, 15, 13, 15 ,17 ,17,
							17, 17, 19, 19, 19, 19, 19, 21,
							21, 21, 21, 21, 21, 22, 22, 22,
							22, 22, 22, 22, 24, 24, 24, 24,
							24, 24, 24, 24, 25, 25, 25, 25,
							25, 25, 25, 27, 27, 27, 27, 27,
							27, 28, 28, 28, 28, 28, 30, 30,
							30, 30, 32, 32, 32, 33, 33, 35};

static unsigned int *DefaultScalingList[8]      =
{
	DefaultScalingList4x4Intra,
	DefaultScalingList4x4Intra,
	DefaultScalingList4x4Intra,
	DefaultScalingList4x4Inter,
	DefaultScalingList4x4Inter,
	DefaultScalingList4x4Inter,
	DefaultScalingList8x8Intra,
	DefaultScalingList8x8Inter
};

// -----------------------------------------

static unsigned int DefaultScanOrderScalingList4x4Intra[16];
static unsigned int DefaultScanOrderScalingList4x4Inter[16];
static unsigned int DefaultScanOrderScalingList8x8Intra[64];
static unsigned int DefaultScanOrderScalingList8x8Inter[64];

static unsigned int *DefaultScanOrderScalingList[8]      =
{
	DefaultScanOrderScalingList4x4Intra,
	DefaultScanOrderScalingList4x4Intra,
	DefaultScanOrderScalingList4x4Intra,
	DefaultScanOrderScalingList4x4Inter,
	DefaultScanOrderScalingList4x4Inter,
	DefaultScanOrderScalingList4x4Inter,
	DefaultScanOrderScalingList8x8Intra,
	DefaultScanOrderScalingList8x8Inter
};

// -----------------------------------------

void   FrameParser_VideoH264_c::PrepareScannedScalingLists(	void )
{
unsigned int 	i;

    for( i=0; i<16; i++ )
	DefaultScanOrderScalingList4x4Intra[ZZScan4x4[i]]	= DefaultScalingList4x4Intra[i];
    for( i=0; i<16; i++ )
	DefaultScanOrderScalingList4x4Inter[ZZScan4x4[i]]	= DefaultScalingList4x4Inter[i];
    for( i=0; i<64; i++ )
	DefaultScanOrderScalingList8x8Intra[ZZScan8x8[i]]	= DefaultScalingList8x8Intra[i];
    for( i=0; i<64; i++ )
	DefaultScanOrderScalingList8x8Inter[ZZScan8x8[i]]	= DefaultScalingList8x8Inter[i];
}

// -----------------------------------------

FrameParserStatus_t   FrameParser_VideoH264_c::ReadScalingList( 
						    bool                 ScalingListPresent,
						    unsigned int        *ScalingList,
						    unsigned int        *Default,
						    unsigned int        *Fallback,
						    unsigned int         SizeOfScalingList,
						    unsigned int        *UseDefaultScalingMatrixFlag )
{
unsigned int		 i,j;
unsigned char		*Scan;
int			 LastScale;
int			 NextScale;
int			 delta_scale;

//

    Scan        = (SizeOfScalingList == 16) ? ZZScan4x4 : ZZScan8x8;

//

    if( ScalingListPresent )
    {
	LastScale   = 8;
	NextScale   = 8;

	for( j=0; j<SizeOfScalingList; j++ )
	{
	    if( NextScale != 0 )
	    {
		delta_scale                 = se(v);
		NextScale                   = (LastScale + delta_scale + 256) % 256;
		*UseDefaultScalingMatrixFlag= (j==0) && (NextScale == 0);
	    }

	    ScalingList[Scan[j]]            = (NextScale == 0) ? LastScale : NextScale;
	    LastScale                       = ScalingList[Scan[j]];
	}

	if( *UseDefaultScalingMatrixFlag )
	    for( i=0; i<SizeOfScalingList; i++ )
	    	ScalingList[Scan[i]]        = Default[i];
    }
    else
    {
	for( i=0; i<SizeOfScalingList; i++ )
	    ScalingList[i]        = Fallback[i];
    }

//

#ifdef DUMP_HEADERS
    report( severity_info, "    Scaling List :-\n" );

    report( severity_info, "\tUseDefaultScalingMatrixFlag                = %6d\n", *UseDefaultScalingMatrixFlag );
    report( severity_info, "\tMatrix" );
    for( j=0; j<SizeOfScalingList; j+=8 )
    {
	report( severity_info, "\n\t\t" );
	for( i=0; i<8; i++ )
	    report( severity_info, "%6d, ", ScalingList[j+i] );
    }
    report( severity_info, "\n" );
#endif

//

    AssertAntiEmulationOk( "ReadScalingList" );

//

    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in the Hrd parameters (E.1.2 of the specification)
//

FrameParserStatus_t   FrameParser_VideoH264_c::ReadHrdParameters(   H264HrdParameters_t      *Header )
{
unsigned int		 i;

//

    Header->cpb_cnt_minus1                              = ue(v);

    if( Header->cpb_cnt_minus1 >= H264_HRD_MAX_CPB_CNT )
    {
	report( severity_error, "FrameParser_VideoH264_c::ReadHrdParameters - cpb_cnt_minus1 exceeds our soft restriction (%d,%d).\n", Header->cpb_cnt_minus1, H264_HRD_MAX_CPB_CNT-1 );
	return FrameParserError;
    }

    Header->bit_rate_scale                              = u(4);
    Header->cpb_size_scale                              = u(4);
    for( i=0; i<=Header->cpb_cnt_minus1; i++ )
    {
	Header->bit_rate_value_minus1[i]                = ue(v);
	Header->cpb_size_value_minus1[i]                = ue(v);
	Header->cbr_flag[i]                             = u(1);
    }
    Header->initial_cpb_removal_delay_length_minus1     = u(5);
    Header->cpb_removal_delay_length_minus1             = u(5);
    Header->cpb_output_delay_length_minus1              = u(5);
    Header->time_offset_length                          = u(5);

//

    CpbDpbDelaysPresentFlag                             = 1;
    CpbRemovalDelayLength                               = Header->cpb_removal_delay_length_minus1 + 1;
    DpbOutputDelayLength                                = Header->cpb_output_delay_length_minus1 + 1;
    TimeOffsetLength                                    = Header->time_offset_length;

//

#ifdef DUMP_HEADERS
    report( severity_info, "    HRD Parameter Header :-\n" );

    report( severity_info, "\tcpb_cnt_minus1                             = %6d\n", Header->cpb_cnt_minus1 );
    report( severity_info, "\tbit_rate_scale                             = %6d\n", Header->bit_rate_scale );
    report( severity_info, "\tcpb_size_scale                             = %6d\n", Header->cpb_size_scale );
    for( i=0; i<=Header->cpb_cnt_minus1; i++ )
    {
	report( severity_info, "\tbit_rate_value_minus1[%3d]                 = %6d\n", i, Header->bit_rate_value_minus1[i] );
	report( severity_info, "\tcpb_size_value_minus1[%3d]                 = %6d\n", i, Header->cpb_size_value_minus1[i] );
	report( severity_info, "\tcbr_flag[%3d]                              = %6d\n", i, Header->cbr_flag[i] );
    }
    report( severity_info, "\tinitial_cpb_removal_delay_length_minus1    = %6d\n", Header->initial_cpb_removal_delay_length_minus1 );
    report( severity_info, "\tcpb_removal_delay_length_minus1            = %6d\n", Header->cpb_removal_delay_length_minus1 );
    report( severity_info, "\tcpb_output_delay_length_minus1             = %6d\n", Header->cpb_output_delay_length_minus1 );
    report( severity_info, "\ttime_offset_length                         = %6d\n", Header->time_offset_length );

#endif

//

    AssertAntiEmulationOk( "ReadHrdParameters" );

//

    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in the VUI sequence parameters (E.1.1 of the specification)
//

FrameParserStatus_t   FrameParser_VideoH264_c::ReadVUISequenceParameters( H264VUISequenceParameters_t       *Header )
{
FrameParserStatus_t   Status;

//

    Header->aspect_ratio_info_present_flag              = u(1);
    if( Header->aspect_ratio_info_present_flag )
    {
	Header->aspect_ratio_idc                        = u(8);
	if( Header->aspect_ratio_idc == H264_ASPECT_RATIO_EXTENDED_SAR )
	{
	    Header->sar_width                           = u(16);
	    Header->sar_height                          = u(16);
	}
    }

//

    Header->overscan_info_present_flag                  = u(1);
    if( Header->overscan_info_present_flag )
	Header->overscan_appropriate_flag               = u(1);

//

    Header->video_signal_type_present_flag              = u(1);
    if( Header->video_signal_type_present_flag )
    {
	Header->video_format                            = u(3);
	Header->video_full_range_flag                   = u(1);
	Header->colour_description_present_flag         = u(1);
	if( Header->colour_description_present_flag )
	{
	    Header->colour_primaries                    = u(8);
	    Header->transfer_characteristics            = u(8);
	    Header->matrix_coefficients                 = u(8);
	}
    }

//

    Header->chroma_loc_info_present_flag                = u(1);
    if( Header->chroma_loc_info_present_flag )
    {
	Header->chroma_sample_loc_type_top_field        = ue(v);
	Header->chroma_sample_loc_type_bottom_field     = ue(v);
    }

//

    Header->timing_info_present_flag                    = u(1);
    if( Header->timing_info_present_flag )
    {
	Header->num_units_in_tick                       = u(32);
	Header->time_scale                              = u(32);
	Header->fixed_frame_rate_flag                   = u(1);
    }

//

    Header->nal_hrd_parameters_present_flag             = u(1);
    if( Header->nal_hrd_parameters_present_flag )
    {
	Status = ReadHrdParameters( &Header->nal_hrd_parameters );
	if( Status != FrameParserNoError )
	    return Status;
    }

    Header->vcl_hrd_parameters_present_flag             = u(1);
    if( Header->vcl_hrd_parameters_present_flag )
    {
	Status = ReadHrdParameters( &Header->vcl_hrd_parameters );
	if( Status != FrameParserNoError )
	    return Status;
    }

    if( Header->nal_hrd_parameters_present_flag ||  Header->vcl_hrd_parameters_present_flag )
	Header->low_delay_hrd_flag                      = u(1);

//

    Header->pict_struct_present_flag                    = u(1);
    Header->bitstream_restriction_flag                  = u(1);
    if( Header->bitstream_restriction_flag )
    {
	Header->motion_vectors_over_pic_boundaries_flag = u(1);
	Header->max_bytes_per_pic_denom                 = ue(v);
	Header->max_bits_per_mb_denom                   = ue(v);
	Header->log2_max_mv_length_horizontal           = ue(v);
	Header->log2_max_mv_length_vertical             = ue(v);
	Header->num_reorder_frames                      = ue(v);
	Header->max_dec_frame_buffering                 = ue(v);
    }

//

    PicStructPresentFlag                                = Header->pict_struct_present_flag;

//

#ifdef DUMP_HEADERS
    report( severity_info, "    VUI Sequence Parameter Header :-\n" );

    report( severity_info, "\taspect_ratio_info_present_flag             = %6d\n", Header->aspect_ratio_info_present_flag );
    if( Header->aspect_ratio_info_present_flag )
    {
	report( severity_info, "\taspect_ratio_idc                           = %6d\n", Header->aspect_ratio_idc );
	if( Header->aspect_ratio_idc == H264_ASPECT_RATIO_EXTENDED_SAR )
	{
	    report( severity_info, "\tsar_width                                  = %6d\n", Header->sar_width );
	    report( severity_info, "\tsar_height                                 = %6d\n", Header->sar_height );
	}
    }

    report( severity_info, "\toverscan_info_present_flag                 = %6d\n", Header->overscan_info_present_flag );
    if( Header->overscan_info_present_flag )
	report( severity_info, "\toverscan_appropriate_flag                  = %6d\n", Header->overscan_appropriate_flag );

    report( severity_info, "\tvideo_signal_type_present_flag             = %6d\n", Header->video_signal_type_present_flag );
    if( Header->video_signal_type_present_flag )
    {
	report( severity_info, "\tvideo_format                               = %6d\n", Header->video_format );
	report( severity_info, "\tvideo_full_range_flag                      = %6d\n", Header->video_full_range_flag );
	report( severity_info, "\tcolour_description_present_flag            = %6d\n", Header->colour_description_present_flag );
	if( Header->colour_description_present_flag )
	{
	    report( severity_info, "\tcolour_primaries                           = %6d\n", Header->colour_primaries );
	    report( severity_info, "\ttransfer_characteristics                   = %6d\n", Header->transfer_characteristics );
	    report( severity_info, "\tmatrix_coefficients                        = %6d\n", Header->matrix_coefficients );
	}
    }

    report( severity_info, "\tchroma_loc_info_present_flag               = %6d\n", Header->chroma_loc_info_present_flag );
    if( Header->chroma_loc_info_present_flag )
    {
	report( severity_info, "\tchroma_sample_loc_type_top_field           = %6d\n", Header->chroma_sample_loc_type_top_field );
	report( severity_info, "\tchroma_sample_loc_type_bottom_field        = %6d\n", Header->chroma_sample_loc_type_bottom_field );
    }

    report( severity_note, "\ttiming_info_present_flag                   = %6d\n", Header->timing_info_present_flag );
    if( Header->timing_info_present_flag )
    {
	report( severity_note, "\tnum_units_in_tick                          = %6d\n", Header->num_units_in_tick );
	report( severity_note, "\ttime_scale                                 = %6d\n", Header->time_scale );
	report( severity_note, "\tfixed_frame_rate_flag                      = %6d\n", Header->fixed_frame_rate_flag );
    }

    report( severity_info, "\tnal_hrd_parameters_present_flag            = %6d\n", Header->nal_hrd_parameters_present_flag );
    report( severity_info, "\tvcl_hrd_parameters_present_flag            = %6d\n", Header->vcl_hrd_parameters_present_flag );
    if( Header->nal_hrd_parameters_present_flag ||  Header->vcl_hrd_parameters_present_flag )
	report( severity_info, "\tlow_delay_hrd_flag                         = %6d\n", Header->low_delay_hrd_flag );

    report( severity_note, "\tpict_struct_present_flag                   = %6d\n", Header->pict_struct_present_flag );
    report( severity_info, "\tbitstream_restriction_flag                 = %6d\n", Header->bitstream_restriction_flag );
    if( Header->bitstream_restriction_flag )
    {
	report( severity_info, "\tmotion_vectors_over_pic_boundaries_flag    = %6d\n", Header->motion_vectors_over_pic_boundaries_flag );
	report( severity_info, "\tmax_bytes_per_pic_denom                    = %6d\n", Header->max_bytes_per_pic_denom );
	report( severity_info, "\tmax_bits_per_mb_denom                      = %6d\n", Header->max_bits_per_mb_denom );
	report( severity_info, "\tlog2_max_mv_length_horizontal              = %6d\n", Header->log2_max_mv_length_horizontal );
	report( severity_info, "\tlog2_max_mv_length_vertical                = %6d\n", Header->log2_max_mv_length_vertical );
	report( severity_info, "\tnum_reorder_frames                         = %6d\n", Header->num_reorder_frames );
	report( severity_info, "\tmax_dec_frame_buffering                    = %6d\n", Header->max_dec_frame_buffering );
    }
#endif

//

    AssertAntiEmulationOk( "ReadVUISequenceParameters" );

//

    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in a Sequence parameter set (7.3.2.1 of the specification)
//

FrameParserStatus_t   FrameParser_VideoH264_c::ReadNalSequenceParameterSet( void )
{
Buffer_t				 SPSBuffer;
SequenceParameterSetPair_t		*HeaderPair;
H264SequenceParameterSetHeader_t        *Header;
FrameParserStatus_t			 Status;
unsigned int                             i;
unsigned int				*FallbackScalingList[8];

    //
    // Get a sequence parameter set
    //

    Status	= SequenceParameterSetPool->GetBuffer( &SPSBuffer );
    if( Status != BufferNoError )
    {
	report( severity_error, "FrameParser_VideoH264_c::ReadNalSequenceParameterSet - Failed to get a sequence parameter set buffer.\n" );
	return Status;
    }

    SPSBuffer->ObtainDataReference( NULL, NULL, (void **)(&HeaderPair) );

//

    memset( HeaderPair, 0x00, sizeof(SequenceParameterSetPair_t) );

    Header		= &HeaderPair->SequenceParameterSetHeader;
    SetDefaultSequenceParameterSet( Header );

//

    Header->profile_idc                                 = u(8);
    Header->constrained_set0_flag                       = u(1);
    Header->constrained_set1_flag                       = u(1);
    Header->constrained_set2_flag                       = u(1);
    Header->constrained_set3_flag                       = u(1);
    MarkerBits(4, 0);
    Header->level_idc                                   = u(8);
    Header->seq_parameter_set_id                        = ue(v);

    if( Header->seq_parameter_set_id >= H264_MAX_SEQUENCE_PARAMETER_SETS )
    {
	report( severity_error, "FrameParser_VideoH264_c::ReadNalSequenceParameterSet - seq_parameter_set_id exceeds our soft restriction (%d,%d).\n", Header->seq_parameter_set_id, H264_MAX_SEQUENCE_PARAMETER_SETS-1 );
	SPSBuffer->DecrementReferenceCount();
	//Player->MarkStreamUnPlayable( Stream );
	return FrameParserError;
    }

//

    if( (Header->profile_idc == H264_PROFILE_IDC_HIGH)    ||
	(Header->profile_idc == H264_PROFILE_IDC_HIGH10)  ||
	(Header->profile_idc == H264_PROFILE_IDC_HIGH422) ||
	(Header->profile_idc == H264_PROFILE_IDC_HIGH444) )
    {
	Header->chroma_format_idc                       = ue(v);
	if( Header->chroma_format_idc == 3 )
	    Header->residual_colour_transform_flag      = u(1);

	Header->bit_depth_luma_minus8                   = ue(v);
	Header->bit_depth_chroma_minus8                 = ue(v);
	Header->qpprime_y_zero_transform_bypass_flag    = u(1);
	Header->seq_scaling_matrix_present_flag         = u(1);

	if( Header->seq_scaling_matrix_present_flag )
	{
	    // Fallback rule A

	    FallbackScalingList[0]  = DefaultScanOrderScalingList[0];
	    FallbackScalingList[1]  = Header->ScalingList4x4[0];
	    FallbackScalingList[2]  = Header->ScalingList4x4[1];
	    FallbackScalingList[3]  = DefaultScanOrderScalingList[3];
	    FallbackScalingList[4]  = Header->ScalingList4x4[3];
	    FallbackScalingList[5]  = Header->ScalingList4x4[4];
	    FallbackScalingList[6]  = DefaultScanOrderScalingList[6];
	    FallbackScalingList[7]  = DefaultScanOrderScalingList[7];

	    for( i=0; i<6; i++ )
	    {
		Header->seq_scaling_list_present_flag[i]=  u(1);
		Status = ReadScalingList(       Header->seq_scaling_list_present_flag[i],
						Header->ScalingList4x4[i], 
						DefaultScalingList[i],
						FallbackScalingList[i],
						16, 
						&Header->UseDefaultScalingMatrix4x4Flag[i] );
		if( Status != FrameParserNoError )
		{
		    SPSBuffer->DecrementReferenceCount();
		    return Status;
		}
	    }
	    for( i=0; i<2; i++ )
	    {
		Header->seq_scaling_list_present_flag[i+6]=  u(1);
		Status = ReadScalingList(       Header->seq_scaling_list_present_flag[i+6],
						Header->ScalingList8x8[i], 
						DefaultScalingList[i+6],
						FallbackScalingList[i+6],
						64, 
						&Header->UseDefaultScalingMatrix8x8Flag[i] );
		if( Status != FrameParserNoError )
		{
		    SPSBuffer->DecrementReferenceCount();
		    return Status;
		}
	    }
	}
    }

//

    Header->log2_max_frame_num_minus4                   = ue(v);
    Header->pic_order_cnt_type                          = ue(v);

    if( Header->pic_order_cnt_type == 0 )
    {
	Header->log2_max_pic_order_cnt_lsb_minus4       = ue(v);
    }
    else if (Header->pic_order_cnt_type == 1 ) // cjt
    {
	Header->delta_pic_order_always_zero_flag        = u(1);
	Header->offset_for_non_ref_pic                  = se(v);
	Header->offset_for_top_to_bottom_field          = se(v);
	Header->num_ref_frames_in_pic_order_cnt_cycle   = ue(v);

	if( Header->num_ref_frames_in_pic_order_cnt_cycle > H264_MAX_REF_FRAMES_IN_PIC_ORDER_CNT_CYCLE )
	{
	    report( severity_error, "FrameParser_VideoH264_c::ReadNalSequenceParameterSet - num_ref_frames_in_pic_order_cnt_cycle exceeds our soft restriction (%d,%d).\n",
			Header->num_ref_frames_in_pic_order_cnt_cycle, H264_MAX_REF_FRAMES_IN_PIC_ORDER_CNT_CYCLE );
	    SPSBuffer->DecrementReferenceCount();
	    Player->MarkStreamUnPlayable( Stream );
	    return FrameParserError;
	}

	for( i=0; i<Header->num_ref_frames_in_pic_order_cnt_cycle; i++ )
	    Header->offset_for_ref_frame[i]             = se(v);
    }

    Header->num_ref_frames                              = ue(v);

    if( Header->num_ref_frames > H264_MAX_REFERENCE_FRAMES)
    {
	report( severity_error,"FrameParser_VideoH264_c::ReadNalSequenceParameterSet - Too many reference frames!!! %d max id %d\n", 
			Header->num_ref_frames, H264_MAX_REFERENCE_FRAMES );
	SPSBuffer->DecrementReferenceCount();
	Player->MarkStreamUnPlayable( Stream );
	return FrameParserError;
    }

    Header->gaps_in_frame_num_value_allowed_flag        = u(1);
    Header->pic_width_in_mbs_minus1                     = ue(v);
    Header->pic_height_in_map_units_minus1              = ue(v);
    Header->frame_mbs_only_flag                         = u(1);

    if( !Header->frame_mbs_only_flag )
	Header->mb_adaptive_frame_field_flag            = u(1);

    Header->direct_8x8_inference_flag                   = u(1);
    Header->frame_cropping_flag                         = u(1);

    if( Header->frame_cropping_flag )
    {
	Header->frame_cropping_rect_left_offset         = ue(v);
	Header->frame_cropping_rect_right_offset        = ue(v);
	Header->frame_cropping_rect_top_offset          = ue(v);
	Header->frame_cropping_rect_bottom_offset       = ue(v);
    }

    Header->vui_parameters_present_flag                 = u(1);
    if( Header->vui_parameters_present_flag )
    {
	Status = ReadVUISequenceParameters( &Header->vui_seq_parameters );
	if( Status != FrameParserNoError )
	{
	    SPSBuffer->DecrementReferenceCount();
	    return Status;
	}
    }

    //
    // This header is complete, if we have one with the same ID then release it,
    // otherwise, just save this one.
    //

    if( SequenceParameterSetTable[Header->seq_parameter_set_id].Buffer != NULL )
	SequenceParameterSetTable[Header->seq_parameter_set_id].Buffer->DecrementReferenceCount();

    SequenceParameterSetTable[Header->seq_parameter_set_id].Buffer		= SPSBuffer;
    SequenceParameterSetTable[Header->seq_parameter_set_id].Header		= Header;
    SequenceParameterSetTable[Header->seq_parameter_set_id].ExtensionHeader	= &HeaderPair->SequenceParameterSetExtensionHeader;

//
// Dump this header
//

    if( CpbDpbDelaysPresentFlag != LastCpbDpbDelaysPresentFlag )
    {
	if( CpbDpbDelaysPresentFlag )
	{
	    unsigned char ForcePicOrderCntValue;

	    ForcePicOrderCntValue		= Player->PolicyValue( Playback, Stream, PolicyH264ForcePicOrderCntIgnoreDpbDisplayFrameOrdering );
	    if( ForcePicOrderCntValue == PolicyValueApply )
		report( severity_info, "H264 Output frame re-ordering should be based on DPB values, \n     but these are being ignored and PicOrderCnt used instead.\n" );
	    else
		report( severity_info, "H264 Output frame re-ordering is based on DPB values.\n" );
	}
	else
	    report( severity_info, "H264 Output frame re-ordering is based on PicOrderCnt values.\n" );

	LastCpbDpbDelaysPresentFlag	= CpbDpbDelaysPresentFlag;
    }

#ifdef DUMP_HEADERS
    report( severity_info, "    Sequence Parameter Header :-\n" );
    report( severity_info, "\tprofile_idc                                = %6d\n", Header->profile_idc );
    report( severity_info, "\tconstrained_set0_flag                      = %6d\n", Header->constrained_set0_flag );
    report( severity_info, "\tconstrained_set1_flag                      = %6d\n", Header->constrained_set1_flag );
    report( severity_info, "\tconstrained_set2_flag                      = %6d\n", Header->constrained_set2_flag );
    report( severity_info, "\tlevel_idc                                  = %6d\n", Header->level_idc );
    report( severity_info, "\tseq_parameter_set_id                       = %6d\n", Header->seq_parameter_set_id );
    report( severity_info, "\tchroma_format_idc                          = %6d\n", Header->chroma_format_idc );
    report( severity_info, "\tresidual_colour_transform_flag             = %6d\n", Header->residual_colour_transform_flag );
    report( severity_info, "\tbit_depth_luma_minus8                      = %6d\n", Header->bit_depth_luma_minus8 );
    report( severity_info, "\tbit_depth_chroma_minus8                    = %6d\n", Header->bit_depth_chroma_minus8 );
    report( severity_info, "\tqpprime_y_zero_transform_bypass_flag       = %6d\n", Header->qpprime_y_zero_transform_bypass_flag );
    report( severity_info, "\tseq_scaling_matrix_present_flag            = %6d\n", Header->seq_scaling_matrix_present_flag );
    report( severity_info, "\t\t%d %d %d %d %d %d %d %d\n",
	Header->seq_scaling_list_present_flag[0], Header->seq_scaling_list_present_flag[1],
	Header->seq_scaling_list_present_flag[2], Header->seq_scaling_list_present_flag[3],
	Header->seq_scaling_list_present_flag[4], Header->seq_scaling_list_present_flag[5],
	Header->seq_scaling_list_present_flag[6], Header->seq_scaling_list_present_flag[7] );
    report( severity_info, "\tlog2_max_frame_num_minus4                  = %6d\n", Header->log2_max_frame_num_minus4 );
    report( severity_info, "\tpic_order_cnt_type                         = %6d\n", Header->pic_order_cnt_type );
    if( Header->pic_order_cnt_type == 0 )
    {
	report( severity_info, "\tlog2_max_pic_order_cnt_lsb_minus4          = %6d\n", Header->log2_max_pic_order_cnt_lsb_minus4 );
    }
    else
    {
	report( severity_info, "\tdelta_pic_order_always_zero_flag           = %6d\n", Header->delta_pic_order_always_zero_flag );
	report( severity_info, "\toffset_for_non_ref_pic                     = %6d\n", Header->offset_for_non_ref_pic );
	report( severity_info, "\toffset_for_top_to_bottom_field             = %6d\n", Header->offset_for_top_to_bottom_field );
	report( severity_info, "\tnum_ref_frames_in_pic_order_cnt_cycle      = %6d\n", Header->num_ref_frames_in_pic_order_cnt_cycle );
	for( i=0; i<Header->num_ref_frames_in_pic_order_cnt_cycle; i++ )
	    report( severity_info, "\toffset_for_ref_frame[%3d]                  = %6d\n", i, Header->offset_for_ref_frame[i] );
    }
    report( severity_info, "\tnum_ref_frames                             = %6d\n", Header->num_ref_frames );
    report( severity_info, "\tgaps_in_frame_num_value_allowed_flag       = %6d\n", Header->gaps_in_frame_num_value_allowed_flag );
    report( severity_info, "\tpic_width_in_mbs_minus1                    = %6d\n", Header->pic_width_in_mbs_minus1 );
    report( severity_info, "\tpic_height_in_map_units_minus1             = %6d\n", Header->pic_height_in_map_units_minus1 );
    report( severity_info, "\tframe_mbs_only_flag                        = %6d\n", Header->frame_mbs_only_flag );
    if( !Header->frame_mbs_only_flag )
	report( severity_info, "\tmb_adaptive_frame_field_flag               = %6d\n", Header->mb_adaptive_frame_field_flag );
    report( severity_info, "\tdirect_8x8_inference_flag                  = %6d\n", Header->direct_8x8_inference_flag );
    report( severity_info, "\tframe_cropping_flag                        = %6d\n", Header->frame_cropping_flag );
    if( Header->frame_cropping_flag )
    {
	report( severity_info, "\tframe_cropping_rect_left_offset            = %6d\n", Header->frame_cropping_rect_left_offset );
	report( severity_info, "\tframe_cropping_rect_right_offset           = %6d\n", Header->frame_cropping_rect_right_offset );
	report( severity_info, "\tframe_cropping_rect_top_offset             = %6d\n", Header->frame_cropping_rect_top_offset );
	report( severity_info, "\tframe_cropping_rect_bottom_offset          = %6d\n", Header->frame_cropping_rect_bottom_offset );
    }
    report( severity_info, "\tvui_parameters_present_flag                = %6d\n", Header->vui_parameters_present_flag );
#endif

//

    AssertAntiEmulationOk( "ReadNalSequenceParameterSet" );

    Assert( inrange(Header->chroma_format_idc, 0, 3) );
    Assert( inrange(Header->bit_depth_luma_minus8, 0, 4) );
    Assert( inrange(Header->bit_depth_chroma_minus8, 0, 4) );
    Assert( inrange(Header->log2_max_frame_num_minus4, 0, 12) );
    Assert( inrange(Header->pic_order_cnt_type, 0, 2) );
    Assert( inrange(Header->log2_max_pic_order_cnt_lsb_minus4, 0, 12) );
    Assert( inrange(Header->num_ref_frames_in_pic_order_cnt_cycle, 0, 255) );

//

    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in a Sequence parameter set extension
//

FrameParserStatus_t   FrameParser_VideoH264_c::ReadNalSequenceParameterSetExtension( void )
{
unsigned int					 seq_parameter_set_id;
H264SequenceParameterSetExtensionHeader_t       *Header;

//

    seq_parameter_set_id                        = ue(v);
    Assert( inrange(seq_parameter_set_id, 0, H264_STANDARD_MAX_SEQUENCE_PARAMETER_SETS-1) );
    if( SequenceParameterSetTable[seq_parameter_set_id].Buffer == NULL )
    {
	report( severity_error, "FrameParser_VideoH264_c::ReadNalSequenceParameterSetExtension - No appropriate sequence parameter set seen.\n" );
	return FrameParserError;
    }

    Header	= SequenceParameterSetTable[seq_parameter_set_id].ExtensionHeader;

//

    memset( Header, 0x00, sizeof(H264SequenceParameterSetExtensionHeader_t) );

    Header->seq_parameter_set_id                        = seq_parameter_set_id;
    Header->aux_format_idc                              = ue(v);

    if( Header->aux_format_idc != 0 )
    {
	Header->bit_depth_aux_minus8                    = ue(v);
	Header->alpha_incr_flag                         = u(1);
	Header->alpha_opaque_value                      = u(Header->bit_depth_aux_minus8 + 9);
	Header->alpha_transparent_value                 = u(Header->bit_depth_aux_minus8 + 9);
    }

    Header->additional_extension_flag                   = u(1);
    if( Header->additional_extension_flag )
    {
	report( severity_error, "FrameParser_VideoH264_c::ReadNalSequenceParameterSetExtension - Additional extension not supported.\n" );
    }

//
// Dump this header
//

#ifdef DUMP_HEADERS
    report( severity_info, "    Sequence Parameter Extension Header :-\n" );
    report( severity_info, "\tseq_parameter_set_id                       = %6d\n", Header->seq_parameter_set_id );
    report( severity_info, "\taux_format_idc                             = %6d\n", Header->aux_format_idc );
    if( Header->aux_format_idc != 0 )
    {
	report( severity_info, "\tbit_depth_aux_minus8                       = %6d\n", Header->bit_depth_aux_minus8 );
	report( severity_info, "\talpha_incr_flag                            = %6d\n", Header->alpha_incr_flag );
	report( severity_info, "\talpha_opaque_value                         = %6d\n", Header->alpha_opaque_value );
	report( severity_info, "\talpha_transparent_value                    = %6d\n", Header->alpha_transparent_value );
    }
    report( severity_info, "\tadditional_extension_flag                  = %6d\n", Header->additional_extension_flag );
#endif

//

    AssertAntiEmulationOk( "ReadNalSequenceParameterSetExtension" );

    Assert( inrange(Header->bit_depth_aux_minus8, 0, 4) );

//

    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in a Picture parameter set (7.3.2.2 of the specification)
//

FrameParserStatus_t   FrameParser_VideoH264_c::ReadNalPictureParameterSet( void )
{
unsigned int                             i;
FrameParserStatus_t			 Status;
Buffer_t				 PPSBuffer;
H264PictureParameterSetHeader_t		*Header;
unsigned int				 BitsPerId;
unsigned int				*FallbackScalingList[8];


    //
    // Get a picture parameter set
    //

    Status	= PictureParameterSetPool->GetBuffer( &PPSBuffer );
    if( Status != BufferNoError )
    {
	report( severity_error, "FrameParser_VideoH264_c::ReadNalPictureParameterSet - Failed to get a picture parameter set buffer.\n" );
	return Status;
    }

    PPSBuffer->ObtainDataReference( NULL, NULL, (void **)(&Header) );

//

    memset( Header, 0x00, sizeof(H264PictureParameterSetHeader_t) );

    Header->pic_parameter_set_id                        = ue(v);
    Header->seq_parameter_set_id                        = ue(v);
    Header->entropy_coding_mode_flag                    = u(1);
    Header->pic_order_present_flag                      = u(1);
    Header->num_slice_groups_minus1                     = ue(v);

//

    Assert( inrange(Header->pic_parameter_set_id, 0, H264_STANDARD_MAX_PICTURE_PARAMETER_SETS-1) );
    Assert( inrange(Header->seq_parameter_set_id, 0, H264_STANDARD_MAX_SEQUENCE_PARAMETER_SETS-1) );
    if( SequenceParameterSetTable[Header->seq_parameter_set_id].Buffer == NULL )
    {
	report( severity_error, "FrameParser_VideoH264_c::ReadNalPictureParameterSet - Appropriate sequence parameter set not found (%d).\n", Header->seq_parameter_set_id );
	PPSBuffer->DecrementReferenceCount();
	return FrameParserError;
    }

    PPSBuffer->AttachBuffer( SequenceParameterSetTable[Header->seq_parameter_set_id].Buffer );
    Header->SequenceParameterSet                = SequenceParameterSetTable[Header->seq_parameter_set_id].Header;
    Header->SequenceParameterSetExtension       = SequenceParameterSetTable[Header->seq_parameter_set_id].ExtensionHeader;

//

    if( Header->num_slice_groups_minus1 >= H264_MAX_SLICE_GROUPS )
    {
	report( severity_error, "FrameParser_VideoH264_c::ReadNalPictureParameterSet - num_slice_groups_minus1 exceeds our soft restriction (%d,%d).\n",
			Header->num_slice_groups_minus1, H264_MAX_SLICE_GROUPS - 1 );
	PPSBuffer->DecrementReferenceCount();
	return FrameParserError;
    }

    if( Header->num_slice_groups_minus1 > 0 )
    {
	Header->slice_group_map_type                    = ue(v);
	if( Header->slice_group_map_type == H264_SLICE_GROUP_MAP_INTERLEAVED )
	{
	    for( i=0; i<=Header->num_slice_groups_minus1; i++ )
		Header->run_length_minus1[i]            = ue(v);
	}
	else if( Header->slice_group_map_type == H264_SLICE_GROUP_MAP_FOREGROUND_AND_LEFTOVER )
	{
	    for( i=0; i<=Header->num_slice_groups_minus1; i++ )
	    {
		Header->top_left[i]                     = ue(v);
		Header->bottom_right[i]                 = ue(v);
	    }
	}
	else if( (Header->slice_group_map_type == H264_SLICE_GROUP_MAP_CHANGING0) ||
		 (Header->slice_group_map_type == H264_SLICE_GROUP_MAP_CHANGING1) ||
		 (Header->slice_group_map_type == H264_SLICE_GROUP_MAP_CHANGING2) )
	{
	    Header->slice_group_change_direction_flag   = u(1);
	    Header->slice_group_change_rate_minus1      = ue(v);
	}
	else if( Header->slice_group_map_type == H264_SLICE_GROUP_MAP_EXPLICIT )
	{
	    Header->pic_size_in_map_units_minus1        = ue(v);

	    if( Header->pic_size_in_map_units_minus1 >= H264_MAX_PIC_SIZE_IN_MAP_UNITS )
	    {
		report( severity_error, "FrameParser_VideoH264_c::ReadNalPictureParameterSet - num_slice_group_map_units_minus1 exceeds our soft restriction (%d,%d).\n",
				Header->pic_size_in_map_units_minus1, H264_MAX_PIC_SIZE_IN_MAP_UNITS - 1 );
		PPSBuffer->DecrementReferenceCount();
		return FrameParserError;
	    }

	    BitsPerId   = Log2Ceil(Header->num_slice_groups_minus1+1);
	    for( i=0; i<=Header->pic_size_in_map_units_minus1; i++ )
		Header->slice_group_id[i]       = u(BitsPerId);
	}
    }

    Header->num_ref_idx_l0_active_minus1                = ue(v);
    Header->num_ref_idx_l1_active_minus1                = ue(v);
    Header->weighted_pred_flag                          = u(1);
    Header->weighted_bipred_idc                         = u(2);
    Header->pic_init_qp_minus26                         = se(v);
    Header->pic_init_qs_minus26                         = se(v);
    Header->chroma_qp_index_offset                      = se(v);
    Header->deblocking_filter_control_present_flag      = u(1);
    Header->constrained_intra_pred_flag                 = u(1);
    Header->redundant_pic_cnt_present_flag              = u(1);

//

    if( (Header->num_ref_idx_l0_active_minus1 >= H264_MAX_REF_L0_IDX_ACTIVE) ||
	(Header->num_ref_idx_l1_active_minus1 >= H264_MAX_REF_L1_IDX_ACTIVE) )
    {
	report( severity_error, "FrameParser_VideoH264_c::ReadNalPictureParameterSet - num_ref_idx_l?_active_minus1 out of supported range (%d > %d or %d > %d)\n",
		Header->num_ref_idx_l0_active_minus1,   H264_MAX_REF_L0_IDX_ACTIVE-1,
		Header->num_ref_idx_l1_active_minus1,   H264_MAX_REF_L1_IDX_ACTIVE-1 );
	PPSBuffer->DecrementReferenceCount();
	return FrameParserError;
    }

//

    // Apply the default for the second QP index

    Header->second_chroma_qp_index_offset               = Header->chroma_qp_index_offset;

//

    if( Bits.MoreRsbpData() )
    {
	Header->transform_8x8_mode_flag                 = u(1);
	Header->pic_scaling_matrix_present_flag         = u(1);

	if( Header->pic_scaling_matrix_present_flag )
	{
	    if( Header->SequenceParameterSet->seq_scaling_matrix_present_flag )
	    {
		// Fallback rule B

 		FallbackScalingList[0]  = Header->SequenceParameterSet->ScalingList4x4[0];
		FallbackScalingList[1]  = Header->ScalingList4x4[0];
		FallbackScalingList[2]  = Header->ScalingList4x4[1];
		FallbackScalingList[3]  = Header->SequenceParameterSet->ScalingList4x4[3];
		FallbackScalingList[4]  = Header->ScalingList4x4[3];
		FallbackScalingList[5]  = Header->ScalingList4x4[4];
		FallbackScalingList[6]  = Header->SequenceParameterSet->ScalingList8x8[0];
		FallbackScalingList[7]  = Header->SequenceParameterSet->ScalingList8x8[1];
	    }
	    else
	    {
		// Fallback rule A

		FallbackScalingList[0]  = DefaultScanOrderScalingList[0];
		FallbackScalingList[1]  = Header->ScalingList4x4[0];
		FallbackScalingList[2]  = Header->ScalingList4x4[1];
		FallbackScalingList[3]  = DefaultScanOrderScalingList[3];
		FallbackScalingList[4]  = Header->ScalingList4x4[3];
		FallbackScalingList[5]  = Header->ScalingList4x4[4];
		FallbackScalingList[6]  = DefaultScanOrderScalingList[6];
		FallbackScalingList[7]  = DefaultScanOrderScalingList[7];
	    }

	    for( i=0; i<6; i++ )
	    {
		Header->pic_scaling_list_present_flag[i]=  u(1);
		Status = ReadScalingList(       Header->pic_scaling_list_present_flag[i],
						Header->ScalingList4x4[i], 
						DefaultScalingList[i],
						FallbackScalingList[i],
						16, 
						&Header->UseDefaultScalingMatrix4x4Flag[i] );
		if( Status != FrameParserNoError )
		{
		    PPSBuffer->DecrementReferenceCount();
		    return Status;
		}
	    }

	    if( Header->transform_8x8_mode_flag )
	    {
		for( i=0; i<2; i++ )
		{
		    Header->pic_scaling_list_present_flag[i+6]=  u(1);
		    Status = ReadScalingList(   Header->pic_scaling_list_present_flag[i+6],
						Header->ScalingList8x8[i], 
						DefaultScalingList[i+6],
						FallbackScalingList[i+6],
						64, 
						&Header->UseDefaultScalingMatrix8x8Flag[i] );
		    if( Status != FrameParserNoError )
		    {
			PPSBuffer->DecrementReferenceCount();
			return Status;
		    }
		}
	    }
	}

	Header->second_chroma_qp_index_offset           = se(v);
    }
//    report( severity_info, "FrameParser_VideoH264_c::ReadNalPictureParameterSet ( %d %d %d )\n", Header->transform_8x8_mode_flag, Header->pic_scaling_matrix_present_flag, Header->second_chroma_qp_index_offset );

    //
    // This header is complete, if we have one with the same ID then release it,
    // otherwise, just save this one.
    //

    if( PictureParameterSetTable[Header->pic_parameter_set_id].Buffer != NULL )
	PictureParameterSetTable[Header->pic_parameter_set_id].Buffer->DecrementReferenceCount();

    PictureParameterSetTable[Header->pic_parameter_set_id].Buffer		= PPSBuffer;
    PictureParameterSetTable[Header->pic_parameter_set_id].Header		= Header;

//
// Dump this header
//

#ifdef DUMP_HEADERS
    report( severity_info, "    Picture Parameter Header :-\n" );
    report( severity_info, "\tpic_parameter_set_id                       = %6d\n", Header->pic_parameter_set_id );
    report( severity_info, "\tseq_parameter_set_id                       = %6d\n", Header->seq_parameter_set_id );
    report( severity_info, "\tentropy_coding_mode_flag                   = %6d\n", Header->entropy_coding_mode_flag );
    report( severity_info, "\tpic_order_present_flag                     = %6d\n", Header->pic_order_present_flag );
    report( severity_info, "\tnum_slice_groups_minus1                    = %6d\n", Header->num_slice_groups_minus1 );
    if( Header->num_slice_groups_minus1 > 0 )
    {
	report( severity_info, "\tslice_group_map_type                       = %6d\n", Header->slice_group_map_type );
	if( Header->slice_group_map_type == H264_SLICE_GROUP_MAP_INTERLEAVED )
	{
	    for( i=0; i<=Header->num_slice_groups_minus1; i++ )
		report( severity_info, "\trun_length_minus1[%3d]                     = %6d\n", i, Header->run_length_minus1[i] );
	}
	else if( Header->slice_group_map_type == H264_SLICE_GROUP_MAP_FOREGROUND_AND_LEFTOVER )
	{
	    for( i=0; i<=Header->num_slice_groups_minus1; i++ )
	    {
		report( severity_info, "\ttop_left[%3d]                              = %6d\n", i, Header->top_left[i] );
		report( severity_info, "\tbottom_right[%3d]                          = %6d\n", i, Header->bottom_right[i] );
	    }
	}
	else if( (Header->slice_group_map_type == H264_SLICE_GROUP_MAP_CHANGING0) ||
		 (Header->slice_group_map_type == H264_SLICE_GROUP_MAP_CHANGING1) ||
		 (Header->slice_group_map_type == H264_SLICE_GROUP_MAP_CHANGING2) )
	{
	    report( severity_info, "\tslice_group_change_direction_flag          = %6d\n", Header->slice_group_change_direction_flag );
	    report( severity_info, "\tslice_group_change_rate_minus1             = %6d\n", Header->slice_group_change_rate_minus1 );
	}
	else if( Header->slice_group_map_type == H264_SLICE_GROUP_MAP_EXPLICIT )
	{
	    report( severity_info, "\tpic_size_in_map_units_minus1               = %6d\n", Header->pic_size_in_map_units_minus1 );
	    for( i=0; i<=Header->pic_size_in_map_units_minus1; i++ )
		report( severity_info, "\tslice_group_id[%3d]                        = %6d\n", i, Header->slice_group_id[i] );
	}
    }
    report( severity_info, "\tnum_ref_idx_l0_active_minus1               = %6d\n", Header->num_ref_idx_l0_active_minus1 );
    report( severity_info, "\tnum_ref_idx_l1_active_minus1               = %6d\n", Header->num_ref_idx_l1_active_minus1 );
    report( severity_info, "\tweighted_pred_flag                         = %6d\n", Header->weighted_pred_flag );
    report( severity_info, "\tweighted_bipred_idc                        = %6d\n", Header->weighted_bipred_idc );
    report( severity_info, "\tpic_init_qp_minus26                        = %6d\n", Header->pic_init_qp_minus26 );
    report( severity_info, "\tpic_init_qs_minus26                        = %6d\n", Header->pic_init_qs_minus26 );
    report( severity_info, "\tchroma_qp_index_offset                     = %6d\n", Header->chroma_qp_index_offset );
    report( severity_info, "\tdeblocking_filter_control_present_flag     = %6d\n", Header->deblocking_filter_control_present_flag );
    report( severity_info, "\tconstrained_intra_pred_flag                = %6d\n", Header->constrained_intra_pred_flag );
    report( severity_info, "\tredundant_pic_cnt_present_flag             = %6d\n", Header->redundant_pic_cnt_present_flag );

    report( severity_info, "\ttransform_8x8_mode_flag                    = %6d\n", Header->transform_8x8_mode_flag );
    report( severity_info, "\tpic_scaling_matrix_present_flag            = %6d\n", Header->pic_scaling_matrix_present_flag );
    report( severity_info, "\t\t%d %d %d %d %d %d %d %d\n",
	Header->pic_scaling_list_present_flag[0], Header->pic_scaling_list_present_flag[1],
	Header->pic_scaling_list_present_flag[2], Header->pic_scaling_list_present_flag[3],
	Header->pic_scaling_list_present_flag[4], Header->pic_scaling_list_present_flag[5],
	Header->pic_scaling_list_present_flag[6], Header->pic_scaling_list_present_flag[7] );
    report( severity_info, "\tsecond_chroma_qp_index_offset              = %6d\n", Header->second_chroma_qp_index_offset );
#endif

//

    AssertAntiEmulationOk( "ReadNalPictureParameterSet" );

    Assert( inrange(Header->slice_group_map_type, 0, 6) );
    Assert( inrange(Header->num_ref_idx_l0_active_minus1, 0, 31) );
    Assert( inrange(Header->num_ref_idx_l1_active_minus1, 0, 31) );
    Assert( inrange(Header->weighted_bipred_idc, 0, 2) );
    Assert( Header->pic_init_qp_minus26 <= 25 );
    Assert( inrange(Header->pic_init_qs_minus26, -26, 25) );
    Assert( inrange(Header->chroma_qp_index_offset, -12, 12) );
    Assert( inrange(Header->second_chroma_qp_index_offset, -12, 12) );

//

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////////
//
//    Private - function responsible for reading reference picture re-ordering data (7.3.3.1)
//

FrameParserStatus_t   FrameParser_VideoH264_c::ReadRefPicListReordering( void )
{
unsigned int			 i;
H264RefPicListReordering_t	*Header;

//

    Header      = &SliceHeader->ref_pic_list_reordering;

    if( !SLICE_TYPE_IS( SliceHeader->slice_type, H264_SLICE_TYPE_I) && !SLICE_TYPE_IS( SliceHeader->slice_type, H264_SLICE_TYPE_SI) )
    {
	Header->ref_pic_list_reordering_flag_l0                         = u(1);
	if( Header->ref_pic_list_reordering_flag_l0 )
	{
	    CheckAntiEmulationBuffer( REF_LIST_REORDER_ANTI_EMULATION_REQUEST );

	    for( i=0; i<H264_MAX_REF_PIC_REORDER_OPERATIONS; i++ )
	    {
		Header->l0_reordering_of_pic_nums_idc[i]                = ue(v);
		Assert( inrange(Header->l0_reordering_of_pic_nums_idc[i], 0, 3) );

		if( (Header->l0_reordering_of_pic_nums_idc[i] == 0) ||
		    (Header->l0_reordering_of_pic_nums_idc[i] == 1) )
		    Header->l0_abs_diff_pic_num_minus1[i]               = ue(v);
		else if( Header->l0_reordering_of_pic_nums_idc[i] == 2 )
		    Header->l0_long_term_pic_num[i]                     = ue(v);
		else if( Header->l0_reordering_of_pic_nums_idc[i] == 3 )
		    break;
	    }

	    if( i >= H264_MAX_REF_PIC_REORDER_OPERATIONS )
	    {
		report( severity_error, "FrameParser_VideoH264_c::ReadRefPicListReordering - Too many L0 list re-ordering commands - exceeds soft restriction (%d).\n", H264_MAX_REF_PIC_REORDER_OPERATIONS );
		return FrameParserError;
	    }
	}
    }

    if( SLICE_TYPE_IS( SliceHeader->slice_type, H264_SLICE_TYPE_B) )
    {
	Header->ref_pic_list_reordering_flag_l1                         = u(1);

	if( Header->ref_pic_list_reordering_flag_l1 )
	{
	    CheckAntiEmulationBuffer( REF_LIST_REORDER_ANTI_EMULATION_REQUEST );

	    for( i=0; i<H264_MAX_REF_PIC_REORDER_OPERATIONS; i++ )
	    {
		Header->l1_reordering_of_pic_nums_idc[i]                = ue(v);
		Assert( inrange(Header->l1_reordering_of_pic_nums_idc[i], 0, 3) );

		if( (Header->l1_reordering_of_pic_nums_idc[i] == 0) ||
		    (Header->l1_reordering_of_pic_nums_idc[i] == 1) )
		    Header->l1_abs_diff_pic_num_minus1[i]               = ue(v);
		else if( Header->l1_reordering_of_pic_nums_idc[i] == 2 )
		    Header->l1_long_term_pic_num[i]                     = ue(v);
		else if( Header->l1_reordering_of_pic_nums_idc[i] == 3 )
		    break;
	    }

	    if( i >= H264_MAX_REF_PIC_REORDER_OPERATIONS )
	    {
		report( severity_error, "FrameParser_VideoH264_c::ReadRefPicListReordering - Too many L1 list re-ordering commands - exceeds soft restriction (%d).\n", H264_MAX_REF_PIC_REORDER_OPERATIONS );
		return FrameParserError;
	    }
	}
    }

//
// Dump this header
//

#ifdef DUMP_HEADERS
    report( severity_info, "    Reference picture re-ordering Header :-\n" );
    if( !SLICE_TYPE_IS( SliceHeader->slice_type, H264_SLICE_TYPE_I) && !SLICE_TYPE_IS( SliceHeader->slice_type, H264_SLICE_TYPE_SI) )
    {
	report( severity_info, "\tref_pic_list_reordering_flag_l0            = %6d\n", Header->ref_pic_list_reordering_flag_l0 );
	if( Header->ref_pic_list_reordering_flag_l0 )
	    for( i=0; i<H264_MAX_REF_L0_IDX_ACTIVE; i++ )
	    {
		report( severity_info, "\tl0_reordering_of_pic_nums_idc[%3d]         = %6d\n", i, Header->l0_reordering_of_pic_nums_idc[i] );
		report( severity_info, "\tl0_abs_diff_pic_num_minus1[%3d]            = %6d\n", i, Header->l0_abs_diff_pic_num_minus1[i] );
		report( severity_info, "\tl0_long_term_pic_num[%3d]                  = %6d\n", i, Header->l0_long_term_pic_num[i] );
		if( Header->l0_reordering_of_pic_nums_idc[i] == 3 )
		    break;
	    }
    }

    if( SLICE_TYPE_IS( SliceHeader->slice_type, H264_SLICE_TYPE_B) )
    {
	report( severity_info, "\tref_pic_list_reordering_flag_l1            = %6d\n", Header->ref_pic_list_reordering_flag_l1 );
	if( Header->ref_pic_list_reordering_flag_l1 )
	    for( i=0; i<H264_MAX_REF_L1_IDX_ACTIVE; i++ )
	    {
		report( severity_info, "\tl1_reordering_of_pic_nums_idc[%3d]         = %6d\n", i, Header->l1_reordering_of_pic_nums_idc[i] );
		report( severity_info, "\tl1_abs_diff_pic_num_minus1[%3d]            = %6d\n", i, Header->l1_abs_diff_pic_num_minus1[i] );
		report( severity_info, "\tl1_long_term_pic_num[%3d]                  = %6d\n", i, Header->l1_long_term_pic_num[i] );
		if( Header->l1_reordering_of_pic_nums_idc[i] == 3 )
		    break;
	    }
    }
#endif

//

    AssertAntiEmulationOk( "ReadRefPicListReordering" );

//

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////////
//
//    Private - Read the prediction weight tables (7.3.3.2 of the specification)
//

FrameParserStatus_t   FrameParser_VideoH264_c::ReadPredWeightTable( void )
{
H264PredWeightTable_t   *Header;
unsigned int             i;
unsigned int		 num_ref_idx_l0_active_minus1;
unsigned int		 num_ref_idx_l1_active_minus1;

//

    num_ref_idx_l0_active_minus1			= SliceHeader->PictureParameterSet->num_ref_idx_l0_active_minus1;
    num_ref_idx_l1_active_minus1			= SliceHeader->PictureParameterSet->num_ref_idx_l1_active_minus1;
    if( SliceHeader->num_ref_idx_active_override_flag )
    {
	num_ref_idx_l0_active_minus1			= SliceHeader->num_ref_idx_l0_active_minus1;
	num_ref_idx_l1_active_minus1			= SliceHeader->num_ref_idx_l1_active_minus1;
    }

//

    Header      = &SliceHeader->pred_weight_table;

    Header->luma_log2_weight_denom                      = ue(v);

    if( SliceHeader->SequenceParameterSet->chroma_format_idc != 0 )
	Header->chroma_log2_weight_denom		= ue(v);

    CheckAntiEmulationBuffer( PRED_WEIGHT_TABLE_ANTI_EMULATION_REQUEST );

    for( i=0; i<=num_ref_idx_l0_active_minus1; i++ )
    {
	Header->luma_weight_l0_flag[i]                  = u(1);
	if( Header->luma_weight_l0_flag[i] )
	{
	    Header->luma_weight_l0[i]                   = se(v);
	    Header->luma_offset_l0[i]                   = se(v);
	}

	if( SliceHeader->SequenceParameterSet->chroma_format_idc != 0 )
	{
	    Header->chroma_weight_l0_flag[i]		= u(1);
	    if( Header->chroma_weight_l0_flag[i] )
	    {
	    	Header->chroma_weight_l0[i][0]		= se(v);
	    	Header->chroma_offset_l0[i][0]		= se(v);
	    	Header->chroma_weight_l0[i][1]		= se(v);
	    	Header->chroma_offset_l0[i][1]		= se(v);
	    }
	}
    }

    if( SLICE_TYPE_IS( SliceHeader->slice_type, H264_SLICE_TYPE_B) )
    {
	CheckAntiEmulationBuffer( PRED_WEIGHT_TABLE_ANTI_EMULATION_REQUEST );

	for( i=0; i<=num_ref_idx_l1_active_minus1; i++ )
	{
	    Header->luma_weight_l1_flag[i]              = u(1);
	    if( Header->luma_weight_l1_flag[i] )
	    {
		Header->luma_weight_l1[i]               = se(v);
		Header->luma_offset_l1[i]               = se(v);
	    }

	    if( SliceHeader->SequenceParameterSet->chroma_format_idc != 0 )
	    {
	    	Header->chroma_weight_l1_flag[i]	= u(1);
	    	if( Header->chroma_weight_l1_flag[i] )
	    	{
	    	    Header->chroma_weight_l1[i][0]	= se(v);
	    	    Header->chroma_offset_l1[i][0]	= se(v);
	    	    Header->chroma_weight_l1[i][1]	= se(v);
	    	    Header->chroma_offset_l1[i][1]	= se(v);
	    	}
	    }
	}
    }

//
// Dump this header
//

#ifdef DUMP_HEADERS
    report( severity_info, "    Prediction weight table Header :-\n" );

    report( severity_info, "\tluma_log2_weight_denom                     = %6d\n", Header->luma_log2_weight_denom );
    if( SliceHeader->SequenceParameterSet->chroma_format_idc != 0 )
	report( severity_info, "\tchroma_log2_weight_denom                   = %6d\n", Header->chroma_log2_weight_denom );

    for( i=0; i<=num_ref_idx_l0_active_minus1; i++ )
    {
	report( severity_info, "\tluma_weight_l0_flag[%3d]                   = %6d\n", i, Header->luma_weight_l0_flag[i] );
	report( severity_info, "\tluma_weight_l0[%3d]                        = %6d\n", i, Header->luma_weight_l0[i] );
	report( severity_info, "\tluma_offset_l0[%3d]                        = %6d\n", i, Header->luma_offset_l0[i] );
	if( SliceHeader->SequenceParameterSet->chroma_format_idc != 0 )
	{
	    report( severity_info, "\tchroma_weight_l0_flag[%3d]                 = %6d\n", i, Header->chroma_weight_l0_flag[i] );
	    report( severity_info, "\tchroma_weight_l0[%3d][0]                   = %6d\n", i, Header->chroma_weight_l0[i][0] );
	    report( severity_info, "\tchroma_offset_l0[%3d][0]                   = %6d\n", i, Header->chroma_offset_l0[i][0] );
	    report( severity_info, "\tchroma_weight_l0[%3d][1]                   = %6d\n", i, Header->chroma_weight_l0[i][1] );
	    report( severity_info, "\tchroma_offset_l0[%3d][1]                   = %6d\n", i, Header->chroma_offset_l0[i][1] );
	}
    }

    if( SLICE_TYPE_IS( SliceHeader->slice_type, H264_SLICE_TYPE_B) )
	for( i=0; i<=num_ref_idx_l0_active_minus1; i++ )
	{
	    report( severity_info, "\tluma_weight_l1_flag[%3d]                   = %6d\n", i, Header->luma_weight_l1_flag[i] );
	    report( severity_info, "\tluma_weight_l1[%3d]                        = %6d\n", i, Header->luma_weight_l1[i] );
	    report( severity_info, "\tluma_offset_l1[%3d]                        = %6d\n", i, Header->luma_offset_l1[i] );
	    if( SliceHeader->SequenceParameterSet->chroma_format_idc != 0 )
	    {
	    	report( severity_info, "\tchroma_weight_l1_flag[%3d]                 = %6d\n", i, Header->chroma_weight_l1_flag[i] );
	    	report( severity_info, "\tchroma_weight_l1[%3d][0]                   = %6d\n", i, Header->chroma_weight_l1[i][0] );
	    	report( severity_info, "\tchroma_offset_l1[%3d][0]                   = %6d\n", i, Header->chroma_offset_l1[i][0] );
	    	report( severity_info, "\tchroma_weight_l1[%3d][1]                   = %6d\n", i, Header->chroma_weight_l1[i][1] );
	    	report( severity_info, "\tchroma_offset_l1[%3d][1]                   = %6d\n", i, Header->chroma_offset_l1[i][1] );
	    }
	}
#endif

//

    AssertAntiEmulationOk( "ReadPredWeightTable" );

    Assert( inrange(Header->luma_log2_weight_denom, 0, 7) );
    Assert( inrange(Header->chroma_log2_weight_denom, 0, 7) );

//

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////////////////////////////
//
//    The function responsible for reading decoded reference picture marking header (7.3.3.3)
//

FrameParserStatus_t   FrameParser_VideoH264_c::ReadDecRefPicMarking( void )
{
unsigned int             i;
H264DecRefPicMarking_t  *Header;

//

    Header      = &SliceHeader->dec_ref_pic_marking;

    if( SliceHeader->nal_unit_type == NALU_TYPE_IDR )
    {
	Header->no_ouptput_of_prior_pics_flag                           = u(1);
	Header->long_term_reference_flag                                = u(1);
    }
    else
    {
	Header->adaptive_ref_pic_marking_mode_flag                      = u(1);
	if( Header->adaptive_ref_pic_marking_mode_flag )
	{
	    CheckAntiEmulationBuffer( MEM_MANAGEMENT_ANTI_EMULATION_REQUEST );
	    for( i=0; i<H264_MAX_MEMORY_MANAGEMENT_OPERATIONS; i++ )
	    {
		Header->memory_management_control_operation[i]          = ue(v);
		Assert( inrange(Header->memory_management_control_operation[i], 0, 6) );

		if( (Header->memory_management_control_operation[i] == H264_MMC_MARK_SHORT_TERM_UNUSED_FOR_REFERENCE) ||
		    (Header->memory_management_control_operation[i] == H264_MMC_ASSIGN_LONG_TERM_INDEX_TO_SHORT_TERM_PICTURE) )
		    Header->difference_of_pic_nums_minus1[i]            = ue(v);

		if( Header->memory_management_control_operation[i] == H264_MMC_MARK_LONG_TERM_UNUSED_FOR_REFERENCE )
		    Header->long_term_pic_num[i]                        = ue(v);

		if( Header->memory_management_control_operation[i] == H264_MMC_ASSIGN_LONG_TERM_INDEX_TO_SHORT_TERM_PICTURE )
		     Header->long_term_frame_idx[i]                     = ue(v);

		if( Header->memory_management_control_operation[i] == H264_MMC_ASSIGN_LONG_TERM_INDEX_TO_CURRENT_DECODED_PICTURE )
		{
		    Header->long_term_frame_idx[i]                      = ue(v);
		}

		if( Header->memory_management_control_operation[i] == H264_MMC_SPECIFY_MAX_LONG_TERM_INDEX )
		    Header->max_long_term_frame_idx_plus1[i]            = ue(v);

		if( Header->memory_management_control_operation[i] == H264_MMC_END )
		    break;
	    }

	    if( i >= H264_MAX_MEMORY_MANAGEMENT_OPERATIONS )
	    {
		report( severity_error, "FrameParser_VideoH264_c::ReadDecRefPicMarking - Too many memory management control operations - exceeds soft restriction (%d).\n", H264_MAX_MEMORY_MANAGEMENT_OPERATIONS );
		return FrameParserError;
	    }
	}
    }

//
// Dump this header
//

#ifdef DUMP_HEADERS
    report( severity_info, "    Decoded Reference Picture marking Header :-\n" );

    if( SliceHeader->nal_unit_type == NALU_TYPE_IDR )
    {
	report( severity_info, "\tno_ouptput_of_prior_pics_flag              = %6d\n", Header->no_ouptput_of_prior_pics_flag );
	report( severity_info, "\tlong_term_reference_flag                   = %6d\n", Header->long_term_reference_flag );
    }
    else
    {
	report( severity_info, "\tadaptive_ref_pic_marking_mode_flag         = %6d\n", Header->adaptive_ref_pic_marking_mode_flag );

	for( i=0; i<H264_MAX_MEMORY_MANAGEMENT_OPERATIONS; i++ )
	{
	    report( severity_info, "\tmemory_management_control_operation[%3d]   = %6d\n", i, Header->memory_management_control_operation[i] );
	    report( severity_info, "\tdifference_of_pic_nums_minus1[%3d]         = %6d\n", i, Header->difference_of_pic_nums_minus1[i] );
	    report( severity_info, "\tlong_term_pic_num[%3d]                     = %6d\n", i, Header->long_term_pic_num[i] );
	    report( severity_info, "\tlong_term_frame_idx[%3d]                   = %6d\n", i, Header->long_term_frame_idx[i] );
	    report( severity_info, "\tmax_long_term_frame_idx_plus1[%3d]         = %6d\n", i, Header->max_long_term_frame_idx_plus1[i] );

	    if( Header->memory_management_control_operation[i] == H264_MMC_END )
		break;
	}
    }
#endif

//

    AssertAntiEmulationOk( "ReadDecRefPicMarking" );

//

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////////
//
//    The function responsible for reading a slice header (7.3.3)
//

FrameParserStatus_t   FrameParser_VideoH264_c::ReadNalSliceHeader( void )
{
FrameParserStatus_t	  Status;
H264SliceHeader_t	 *Header;
unsigned int		  first_mb_in_slice;
unsigned int		  slice_type;

    //
    // Check the first macroblock in slice field,
    // return if this is not the first slice in a picture.
    //

    first_mb_in_slice					= ue(v);
    slice_type  	                                = ue(v);

    if( first_mb_in_slice != 0 )
    {
	if( (FrameParameters == NULL) || (SliceHeader == NULL) || !SliceHeader->Valid )
	    return FrameParserError;

	//
	// Check for baseline profile with arbitrary slice ordering
	//

	if( (SliceHeader->SequenceParameterSet->profile_idc == H264_PROFILE_IDC_BASELINE) &&
	    (first_mb_in_slice < SliceHeader->first_mb_in_slice) )
	{
	    report( severity_error, "FrameParser_VideoH264_c::ReadNalSliceHeader - Baseline profile with arbitrary slice ordering not supported.\n" );
	    Player->MarkStreamUnPlayable( Stream );
	    return FrameParserError;
	}
//

#ifdef DUMP_HEADERS
    report( severity_info, "    Slice Header :-\n" );
    report( severity_info, "\tfirst_mb_in_slice                          = %6d\n", first_mb_in_slice );
    report( severity_info, "\tslice_type                                 = %6d\n", slice_type );

    if( slice_type != SliceHeader->slice_type )
	report( severity_info, "Slice type mismatch %d - %d.\n", SliceHeader->slice_type, slice_type );
#endif

	if( SliceHeader->slice_type != slice_type )
	    ParsedFrameParameters->IndependentFrame = false;

	return FrameParserNoError;
    }

    //
    // If this is the first slice of a picture, initialize a new slice header
    //

    Status  = GetNewFrameParameters( (void **)&FrameParameters );
    if( Status != FrameParserNoError )
	return Status;

    SliceHeader		= &FrameParameters->SliceHeader;
    Header      	= SliceHeader;
    SetDefaultSliceHeader( Header );

    Header->nal_unit_type                               = nal_unit_type;
    Header->nal_ref_idc                                 = nal_ref_idc;

    Header->first_mb_in_slice                           = first_mb_in_slice;
    Header->slice_type                                  = slice_type;

//

    Header->pic_parameter_set_id                        = ue(v);

    Assert( inrange(Header->pic_parameter_set_id, 0, H264_STANDARD_MAX_PICTURE_PARAMETER_SETS) );
    if( PictureParameterSetTable[Header->pic_parameter_set_id].Buffer == NULL )
    {
	report( severity_error, "FrameParser_VideoH264_c::ReadNalSliceHeader - Appropriate picture parameter set not found (%d).\n", Header->pic_parameter_set_id );
	return FrameParserError;
    }

    Header->PictureParameterSet                         = PictureParameterSetTable[Header->pic_parameter_set_id].Header;
    Header->SequenceParameterSet                        = Header->PictureParameterSet->SequenceParameterSet;
    Header->SequenceParameterSetExtension               = Header->PictureParameterSet->SequenceParameterSetExtension;

//

    Header->frame_num                                   = u(Header->SequenceParameterSet->log2_max_frame_num_minus4 + 4);

    if( !Header->SequenceParameterSet->frame_mbs_only_flag )
    {
	Header->field_pic_flag                          = u(1);
	if( Header->field_pic_flag )
	    Header->bottom_field_flag                   = u(1);
    }

    if( Header->nal_unit_type == NALU_TYPE_IDR )
	Header->idr_pic_id                              = ue(v);

    if( Header->SequenceParameterSet->pic_order_cnt_type == 0 )
    {
	Header->pic_order_cnt_lsb                       = u(Header->SequenceParameterSet->log2_max_pic_order_cnt_lsb_minus4 + 4);
	if( Header->PictureParameterSet->pic_order_present_flag && !Header->field_pic_flag )
	    Header->delta_pic_order_cnt_bottom          = se(v);
    }
    else if( (Header->SequenceParameterSet->pic_order_cnt_type == 1) && !Header->SequenceParameterSet->delta_pic_order_always_zero_flag )
    {
	Header->delta_pic_order_cnt[0]                  = se(v);
	if( Header->PictureParameterSet->pic_order_present_flag && !Header->field_pic_flag )
	    Header->delta_pic_order_cnt[1]              = se(v);
    }

    if( Header->PictureParameterSet->redundant_pic_cnt_present_flag )
	Header->redundant_pic_cnt                       = ue(v);

    if( SLICE_TYPE_IS( Header->slice_type, H264_SLICE_TYPE_B) )
	Header->direct_spatial_mv_pred_flag             = u(1);

    if( SLICE_TYPE_IS( Header->slice_type, H264_SLICE_TYPE_P)  ||
	SLICE_TYPE_IS( Header->slice_type, H264_SLICE_TYPE_SP) ||
	SLICE_TYPE_IS( Header->slice_type, H264_SLICE_TYPE_B) )
    {
	Header->num_ref_idx_active_override_flag        = u(1);
	if( Header->num_ref_idx_active_override_flag )
	{
	    Header->num_ref_idx_l0_active_minus1        = ue(v);
	    if( SLICE_TYPE_IS( Header->slice_type, H264_SLICE_TYPE_B) )
		Header->num_ref_idx_l1_active_minus1    = ue(v);
	}
    }

//

    Status = ReadRefPicListReordering();
    if( Status != FrameParserNoError )
	return Status;

//

    if( (Header->PictureParameterSet->weighted_pred_flag && (SLICE_TYPE_IS( Header->slice_type, H264_SLICE_TYPE_P) || 
							     SLICE_TYPE_IS( Header->slice_type, H264_SLICE_TYPE_SP))) ||
	((Header->PictureParameterSet->weighted_bipred_idc == 1) && SLICE_TYPE_IS( Header->slice_type, H264_SLICE_TYPE_B)) )
    {
	Status = ReadPredWeightTable();
	if( Status != FrameParserNoError )
	    return Status;
    }

    if( Header->nal_ref_idc != 0 )
    {
	Status = ReadDecRefPicMarking();
	if( Status != FrameParserNoError )
	    return Status;
    }

    CheckAntiEmulationBuffer( DEFAULT_ANTI_EMULATION_REQUEST );

    if( Header->PictureParameterSet->entropy_coding_mode_flag &&
	!SLICE_TYPE_IS( Header->slice_type, H264_SLICE_TYPE_I) &&
	!SLICE_TYPE_IS( Header->slice_type, H264_SLICE_TYPE_SI) )
	Header->cabac_init_idc                          = ue(v);

    Header->slice_qp_delta                              = se(v);

    if( SLICE_TYPE_IS( Header->slice_type, H264_SLICE_TYPE_SP) || SLICE_TYPE_IS( Header->slice_type, H264_SLICE_TYPE_SI) )
    {
	if( SLICE_TYPE_IS( Header->slice_type, H264_SLICE_TYPE_SP) )
	    Header->sp_for_switch_flag                  = u(1);

	Header->slice_qs_delta                          = se(v);
    }

    if( Header->PictureParameterSet->deblocking_filter_control_present_flag )
    {
	Header->disable_deblocking_filter_idc           = ue(v);
	if( Header->disable_deblocking_filter_idc != 1 )
	{
	    Header->slice_alpha_c0_offset_div2          = se(v);
	    Header->slice_beta_offset_div2              = se(v);
	}
    }

    if( (Header->PictureParameterSet->num_slice_groups_minus1 > 0) &&
	inrange(Header->PictureParameterSet->slice_group_map_type, H264_SLICE_GROUP_MAP_CHANGING0, H264_SLICE_GROUP_MAP_CHANGING2) )
    {
    unsigned int PicSizeInMapUnits;
    unsigned int SliceGroupChangeRate;

	PicSizeInMapUnits       = (Header->SequenceParameterSet->pic_width_in_mbs_minus1 + 1) * (Header->SequenceParameterSet->pic_height_in_map_units_minus1 + 1);
	SliceGroupChangeRate    = Header->PictureParameterSet->slice_group_change_rate_minus1 + 1;

	Header->slice_group_change_cycle                = u(Log2Ceil( ((PicSizeInMapUnits / SliceGroupChangeRate) + 1) ));
    }


    //
    // In order to cope with BBC broadcast that does not contain IDR frames,
    // or memory control 5 operations, we do some clever initialization of the
    // previous pic order cnt lsb field and pretend we have seen an idr
    //

    if( !BehaveAsIfSeenAnIDR )
    {
	PrevPicOrderCntLsb              = Header->pic_order_cnt_lsb - 1;
	PrevPicOrderCntMsb              = 0;
	BehaveAsIfSeenAnIDR             = true;
	PicOrderCntOffset		= PicOrderCntOffset + PicOrderCntOffsetAdjust;
    }

//
// Dump this header
//

#ifdef DUMP_HEADERS
    report( severity_info, "    Slice Header :-\n" );

    report( severity_info, "\tfirst_mb_in_slice                          = %6d\n", Header->first_mb_in_slice );
    report( severity_info, "\tslice_type                                 = %6d\n", Header->slice_type );
    report( severity_info, "\tpic_parameter_set_id                       = %6d\n", Header->pic_parameter_set_id );
    report( severity_info, "\tframe_num                                  = %6d\n", Header->frame_num );

    if( !Header->SequenceParameterSet->frame_mbs_only_flag )
    {
	report( severity_note, "\tfield_pic_flag                             = %6d\n", Header->field_pic_flag );
	if( Header->field_pic_flag )
	    report( severity_note, "\tbottom_field_flag                          = %6d\n", Header->bottom_field_flag );
    }

    if( Header->nal_unit_type == NALU_TYPE_IDR )
	report( severity_info, "\tidr_pic_id                                 = %6d\n", Header->idr_pic_id );

    if( Header->SequenceParameterSet->pic_order_cnt_type == 0 )
    {
	report( severity_info, "\tpic_order_cnt_lsb                          = %6d\n", Header->pic_order_cnt_lsb );
	if( Header->PictureParameterSet->pic_order_present_flag && !Header->field_pic_flag )
	    report( severity_info, "\tdelta_pic_order_cnt_bottom                 = %6d\n", Header->delta_pic_order_cnt_bottom );
    }
    else if( (Header->SequenceParameterSet->pic_order_cnt_type == 1) && !Header->SequenceParameterSet->delta_pic_order_always_zero_flag )
    {
	report( severity_info, "\tdelta_pic_order_cnt[0]                     = %6d\n", Header->delta_pic_order_cnt[0] );
	if( Header->PictureParameterSet->pic_order_present_flag && !Header->field_pic_flag )
	    report( severity_info, "\tdelta_pic_order_cnt[1]                     = %6d\n", Header->delta_pic_order_cnt[1] );
    }

    if( Header->PictureParameterSet->redundant_pic_cnt_present_flag )
	report( severity_info, "\tredundant_pic_cnt                          = %6d\n", Header->redundant_pic_cnt );

    if( SLICE_TYPE_IS( Header->slice_type, H264_SLICE_TYPE_B) )
	report( severity_info, "\tdirect_spatial_mv_pred_flag                = %6d\n", Header->direct_spatial_mv_pred_flag );

    if( SLICE_TYPE_IS( Header->slice_type, H264_SLICE_TYPE_P)  ||
	SLICE_TYPE_IS( Header->slice_type, H264_SLICE_TYPE_SP) ||
	SLICE_TYPE_IS( Header->slice_type, H264_SLICE_TYPE_B) )
    {
	report( severity_info, "\tnum_ref_idx_active_override_flag           = %6d\n", Header->num_ref_idx_active_override_flag );
	if( Header->num_ref_idx_active_override_flag )
	{
	    report( severity_info, "\tnum_ref_idx_l0_active_minus1               = %6d\n", Header->num_ref_idx_l0_active_minus1 );
	    if( SLICE_TYPE_IS( Header->slice_type, H264_SLICE_TYPE_B) )
		report( severity_info, "\tnum_ref_idx_l1_active_minus1               = %6d\n", Header->num_ref_idx_l1_active_minus1 );
	}
    }

    if( Header->PictureParameterSet->entropy_coding_mode_flag &&
	!SLICE_TYPE_IS( Header->slice_type, H264_SLICE_TYPE_I) &&
	!SLICE_TYPE_IS( Header->slice_type, H264_SLICE_TYPE_SI) )
	report( severity_info, "\tcabac_init_idc                             = %6d\n", Header->cabac_init_idc );

    report( severity_info, "\tslice_qp_delta                             = %6d\n", Header->slice_qp_delta );

    if( SLICE_TYPE_IS( Header->slice_type, H264_SLICE_TYPE_SP) || SLICE_TYPE_IS( Header->slice_type, H264_SLICE_TYPE_SI) )
    {
	if( SLICE_TYPE_IS( Header->slice_type, H264_SLICE_TYPE_SP) )
	    report( severity_info, "\tsp_for_switch_flag                         = %6d\n", Header->sp_for_switch_flag );

	report( severity_info, "\tslice_qs_delta                             = %6d\n", Header->slice_qs_delta );
    }

    if( Header->PictureParameterSet->deblocking_filter_control_present_flag )
    {
	report( severity_info, "\tdisable_deblocking_filter_idc              = %6d\n", Header->disable_deblocking_filter_idc );
	if( Header->disable_deblocking_filter_idc != 1 )
	{
	    report( severity_info, "\tslice_alpha_c0_offset_div2                 = %6d\n", Header->slice_alpha_c0_offset_div2 );
	    report( severity_info, "\tslice_beta_offset_div2                     = %6d\n", Header->slice_beta_offset_div2 );
	}
    }

    if( (Header->PictureParameterSet->num_slice_groups_minus1 > 0) &&
	inrange(Header->PictureParameterSet->slice_group_map_type, H264_SLICE_GROUP_MAP_CHANGING0, H264_SLICE_GROUP_MAP_CHANGING2) )
	report( severity_info, "\tslice_group_change_cycle                   = %6d\n", Header->slice_group_change_cycle );
#endif

//

    Header->Valid                                       = 1;

//

    AssertAntiEmulationOk( "ReadNalSliceHeader" );

    Assert( inrange(Header->slice_type, 0, 9) );
    Assert( inrange(Header->idr_pic_id, 0, 65535) );
    Assert( inrange(Header->num_ref_idx_l0_active_minus1, 0, 31) );
    Assert( inrange(Header->num_ref_idx_l1_active_minus1, 0, 31) );
    Assert( inrange(Header->cabac_init_idc, 0, 2) );
    Assert( (Header->slice_qp_delta <= 51) );
    Assert( inrange(Header->slice_alpha_c0_offset_div2, -6, 6) );
    Assert( inrange(Header->slice_beta_offset_div2, -6, 6) );

//

    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read picture timing message
//

FrameParserStatus_t   FrameParser_VideoH264_c::ReadSeiPictureTimingMessage( void )
{
unsigned int		i;

//

    memset( &SEIPictureTiming, 0x00, sizeof(H264SEIPictureTiming_t) );

    ForceInterlacedProgressive	= false;

    if( CpbDpbDelaysPresentFlag )
    {
	SEIPictureTiming.cpb_removal_delay                      = u(CpbRemovalDelayLength);
	SEIPictureTiming.dpb_output_delay                       = u(DpbOutputDelayLength);
	SeenDpbValue						= true;
    }

    if( PicStructPresentFlag )
    {
	SEIPictureTiming.pic_struct                             = u(4);

	switch( SEIPictureTiming.pic_struct )
	{
	    case SEI_PICTURE_TIMING_PICSTRUCT_FRAME:
	    case SEI_PICTURE_TIMING_PICSTRUCT_TOP_FIELD:
	    case SEI_PICTURE_TIMING_PICSTRUCT_BOTTOM_FIELD:
			SEIPictureTiming.NumClockTS             = 1;
			break;

	    case SEI_PICTURE_TIMING_PICSTRUCT_TOP_BOTTOM:
	    case SEI_PICTURE_TIMING_PICSTRUCT_BOTTOM_TOP:
	    case SEI_PICTURE_TIMING_PICSTRUCT_FRAME_DOUBLING:
			SEIPictureTiming.NumClockTS             = 2;
			break;

	    case SEI_PICTURE_TIMING_PICSTRUCT_TOP_BOTTOM_TOP:
	    case SEI_PICTURE_TIMING_PICSTRUCT_BOTTOM_TOP_BOTTOM:
	    case SEI_PICTURE_TIMING_PICSTRUCT_FRAME_TRIPLING:
			SEIPictureTiming.NumClockTS             = 3;
			break;

	    case 12:
			SEIPictureTiming.NumClockTS             = 2;
			break;

	    default:
			SEIPictureTiming.NumClockTS             = 0;
			report( severity_error, "FrameParser_VideoH264_c::ReadSeiPictureTimingMessage - pic_struct invalid (%d).\n", SEIPictureTiming.pic_struct );
			return FrameParserHeaderSyntaxError;
			break;
	}

	for( i=0; i<SEIPictureTiming.NumClockTS; i++ )
	{
	    SEIPictureTiming.clock_timestamp_flag[i]            = u(1);
	    if( SEIPictureTiming.clock_timestamp_flag[i] )
	    {
		SEIPictureTiming.ct_type[i]                     = u(2);
		SEIPictureTiming.nuit_field_based_flag[i]       = u(1);
		SEIPictureTiming.counting_type[i]               = u(5);
		SEIPictureTiming.full_timestamp_flag[i]         = u(1);
		SEIPictureTiming.discontinuity_flag[i]          = u(1);
		SEIPictureTiming.cnt_dropped_flag[i]            = u(1);
		SEIPictureTiming.n_frames[i]                    = u(8);

		if( SEIPictureTiming.full_timestamp_flag[i] )
		{
		    SEIPictureTiming.seconds_value[i]           = u(6);
		    SEIPictureTiming.minutes_value[i]           = u(6);
		    SEIPictureTiming.hours_value[i]             = u(5);
		}
		else
		{
		    SEIPictureTiming.seconds_flag[i]            = u(1);
		    if( SEIPictureTiming.seconds_flag[i] )
		    {
			SEIPictureTiming.seconds_value[i]       = u(6);
			SEIPictureTiming.minutes_flag[i]        = u(1);
		    }

		    if( SEIPictureTiming.minutes_flag[i] )
		    {
			SEIPictureTiming.minutes_value[i]       = u(6);
			SEIPictureTiming.hours_flag[i]          = u(1);
		    }

		    if( SEIPictureTiming.hours_flag[i] )
			SEIPictureTiming.hours_value[i]         = u(5);

		    if( TimeOffsetLength > 0 )
			SEIPictureTiming.time_offset[i]         = u(TimeOffsetLength);
		}

		if( SEIPictureTiming.ct_type[i] == 0 )
		{
		    ForceInterlacedProgressive	= true;
		    ForcedInterlacedFlag	= false;
		} 
		else if( SEIPictureTiming.ct_type[i] == 1 )
		{
		    ForceInterlacedProgressive	= true;
		    ForcedInterlacedFlag	= true;
		}
	    }
	}
    }

//

    SEIPictureTiming.Valid      = 1;

//

#ifdef DUMP_HEADERS
    report( severity_note, "    Picture Timing Message :-\n" );

    if( CpbDpbDelaysPresentFlag )
    {
	report( severity_note, "\tcpb_removal_delay                          = %6d\n", SEIPictureTiming.cpb_removal_delay );
	report( severity_note, "\tdpb_output_delay                           = %6d\n", SEIPictureTiming.dpb_output_delay );
    }

    if( PicStructPresentFlag )
    {
	report( severity_note, "\tpic_struct                                 = %6d\n", SEIPictureTiming.pic_struct );
	for( i=0; i<SEIPictureTiming.NumClockTS; i++ )
	{
	    report( severity_note, "\tclock_timestamp_flag[%1d]                    = %6d\n", i, SEIPictureTiming.clock_timestamp_flag[i] );
	    if( SEIPictureTiming.clock_timestamp_flag[i] )
	    {
		report( severity_note, "\tct_type[%1d]                                 = %6d\n", i, SEIPictureTiming.ct_type[i] );
		report( severity_note, "\tnuit_field_based_flag[%1d]                   = %6d\n", i, SEIPictureTiming.nuit_field_based_flag[i] );
		report( severity_note, "\tcounting_type[%1d]                           = %6d\n", i, SEIPictureTiming.counting_type[i] );
		report( severity_note, "\tfull_timestamp_flag[%1d]                     = %6d\n", i, SEIPictureTiming.full_timestamp_flag[i] );
		report( severity_note, "\tdiscontinuity_flag[%1d]                      = %6d\n", i, SEIPictureTiming.discontinuity_flag[i] );
		report( severity_note, "\tcnt_dropped_flag[%1d]                        = %6d\n", i, SEIPictureTiming.cnt_dropped_flag[i] );
		report( severity_note, "\tn_frames[%1d]                                = %6d\n", i, SEIPictureTiming.n_frames[i] );
		report( severity_note, "\tseconds_value[%1d]                           = %6d (%d)\n", i, SEIPictureTiming.seconds_value[i], SEIPictureTiming.full_timestamp_flag[i] || SEIPictureTiming.seconds_flag[i] );
		report( severity_note, "\tminutes_value[%1d]                           = %6d (%d)\n", i, SEIPictureTiming.minutes_value[i], SEIPictureTiming.full_timestamp_flag[i] || SEIPictureTiming.minutes_flag[i] );
		report( severity_note, "\thours_value[%1d]                             = %6d (%d)\n", i, SEIPictureTiming.hours_value[i], SEIPictureTiming.full_timestamp_flag[i] || SEIPictureTiming.hours_flag[i] );
		report( severity_note, "\ttime_offset[%1d]                             = %6d (%d)\n", i, SEIPictureTiming.time_offset[i], TimeOffsetLength > 0 );
	    }
	}
    }
#endif

//

    AssertAntiEmulationOk( "ReadSeiPictureTimingMessage" );

//

    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in pan scan message
//

FrameParserStatus_t   FrameParser_VideoH264_c::ReadSeiPanScanMessage(       void )
{
unsigned int		i;

//

    memset( &SEIPanScanRectangle, 0x00, sizeof(H264SEIPanScanRectangle_t) );

    SEIPanScanRectangle.pan_scan_rect_id                        = ue(v);
    SEIPanScanRectangle.pan_scan_rect_cancel_flag               = u(1);

    if( !SEIPanScanRectangle.pan_scan_rect_cancel_flag )
    {
	SEIPanScanRectangle.pan_scan_cnt_minus1                 = ue(v);
	if( (SEIPanScanRectangle.pan_scan_cnt_minus1 + 1) > H264_SEI_MAX_PAN_SCAN_VALUES )
	{
	    report( severity_error, "FrameParser_VideoH264_c::ReadSeiPanScanMessage - pan_scan_cnt_minus1 (%d) out of range (0..2)\n" );
	    return FrameParserHeaderSyntaxError;
	}

	for( i=0; i<=SEIPanScanRectangle.pan_scan_cnt_minus1; i++ )
	{
	    SEIPanScanRectangle.pan_scan_rect_left_offset[i]    = se(v);
	    SEIPanScanRectangle.pan_scan_rect_right_offset[i]   = se(v);
	    SEIPanScanRectangle.pan_scan_rect_top_offset[i]     = se(v);
	    SEIPanScanRectangle.pan_scan_rect_bottom_offset[i]  = se(v);
	}

	SEIPanScanRectangle.pan_scan_rect_repetition_period     = ue(v);
    }

//

    SEIPanScanRectangle.Valid   = 1;

//

#ifdef DUMP_HEADERS
    report( severity_info, "    Pan Scan Rectangle Message :-\n" );

    report( severity_info, "\tpan_scan_rect_id                           = %6d\n", SEIPanScanRectangle.pan_scan_rect_id );
    report( severity_info, "\tpan_scan_rect_cancel_flag                  = %6d\n", SEIPanScanRectangle.pan_scan_rect_cancel_flag );

    if( !SEIPanScanRectangle.pan_scan_rect_cancel_flag )
    {
	report( severity_info, "\tpan_scan_cnt_minus1                        = %6d\n", SEIPanScanRectangle.pan_scan_cnt_minus1 );
	for( i=0; i<=SEIPanScanRectangle.pan_scan_cnt_minus1; i++ )
	{
	    report( severity_info, "\tpan_scan_rect_left_offset[%1d]               = %6d\n", i, SEIPanScanRectangle.pan_scan_rect_left_offset[i] );
	    report( severity_info, "\tpan_scan_rect_right_offset[%1d]              = %6d\n", i, SEIPanScanRectangle.pan_scan_rect_right_offset[i] );
	    report( severity_info, "\tpan_scan_rect_top_offset[%1d]                = %6d\n", i, SEIPanScanRectangle.pan_scan_rect_top_offset[i] );
	    report( severity_info, "\tpan_scan_rect_bottom_offset[%1d]             = %6d\n", i, SEIPanScanRectangle.pan_scan_rect_bottom_offset[i] );
	}
	report( severity_info, "\tpan_scan_rect_repetition_period            = %6d\n", SEIPanScanRectangle.pan_scan_rect_repetition_period );
    }
#endif

//

    AssertAntiEmulationOk( "ReadSeiPanScanMessage" );

//

    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in a scaling list
//

FrameParserStatus_t   FrameParser_VideoH264_c::ReadSeiPayload( 
							unsigned int		  PayloadType,
							unsigned int		  PayloadSize )
{
    switch( PayloadType )
    {
	case SEI_PIC_TIMING:
		return ReadSeiPictureTimingMessage();
		break;

	case SEI_PAN_SCAN_RECT:
		return ReadSeiPanScanMessage();
		break;

	case SEI_BUFFERING_PERIOD:
	case SEI_FILLER_PAYLOAD:
	case SEI_USER_DATA_REGISTERED_ITU_T_T35:
	case SEI_USER_DATA_UNREGISTERED:
	case SEI_RECOVERY_POINT:
	case SEI_DEC_REF_PIC_MARKING_REPETITION:
	case SEI_SPARE_PIC:
	case SEI_SCENE_INFO:
	case SEI_SUB_SEQ_INFO:
	case SEI_SUB_SEQ_LAYER_CHARACTERISTICS:
	case SEI_SUB_SEQ_CHARACTERISTICS:
	case SEI_FULL_FRAME_FREEZE:
	case SEI_FULL_FRAME_FREEZE_RELEASE:
	case SEI_FULL_FRAME_SNAPSHOT:
	case SEI_PROGRESSIVE_REFINEMENT_SEGMENT_START:
	case SEI_PROGRESSIVE_REFINEMENT_SEGMENT_END:
	case SEI_MOTION_CONSTRAINED_SLICE_GROUP_SET:
	case SEI_FILM_GRAIN_CHARACTERISTICS:
	case SEI_DEBLOCKING_FILTER_DISPLAY_PREFERENCE:
	case SEI_STEREO_VIDEO_INFO:
	case SEI_RESERVED_SEI_MESSAGE:
#ifdef DUMP_HEADERS
		report( severity_info, "FrameParser_VideoH264_c::ReadSeiPayload - Unhandled payload type (type = %d - size = %d).\n", PayloadType, PayloadSize );
#endif
		break;

	default:
		report( severity_error, "FrameParser_VideoH264_c::ReadSeiPayload - Unknown payload type (type = %d - size = %d).\n", PayloadType, PayloadSize );
		break;

    }

    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - a supplemental enhancement record
//

FrameParserStatus_t   FrameParser_VideoH264_c::ReadNalSupplementalEnhancementInformation( unsigned int	UnitLength )
{
FrameParserStatus_t	 Status;
unsigned int    	 PayloadType;
unsigned int    	 PayloadSize;
unsigned char   	*Pointer;
unsigned int		 BitsInByte;
unsigned int    	 Value;
unsigned int		 ConsumedSize;

//

    ConsumedSize	= 0;
    do
    {
	PayloadType     = 0;
	PayloadSize     = 0;

//

	for( Value       = u(8);
	     Value      == 0xff;
	     Value       = u(8) )
	{
	    PayloadType += 255;
	    ConsumedSize++;
	}

	ConsumedSize++;
	PayloadType             += Value;

//

	for( Value       = u(8);
	     Value      == 0xff;
	     Value       = u(8) )
	{
	    PayloadSize += 255;
	    ConsumedSize++;
	}

	ConsumedSize++;
	PayloadSize             += Value;

//

	if( PayloadSize > (ANTI_EMULATION_BUFFER_SIZE - 32) )
	{
	    report( severity_error, "FrameParser_VideoH264_c::ReadNalSupplementalEnhancementInformation - Payload size outside supported range (type = %d - size = %d).\n", PayloadType, PayloadSize );
	    break;
	}

//

	ConsumedSize		+= PayloadSize;
	if( ConsumedSize > UnitLength )
	{
	    report( severity_error, "FrameParser_VideoH264_c::ReadNalSupplementalEnhancementInformation - Payload would consume more data than is present (type = %d - size = %d).\n", PayloadType, PayloadSize );
	    break;
	}

//

	CheckAntiEmulationBuffer( min((PayloadSize + 4), (UnitLength + PayloadSize - ConsumedSize)) );
	Bits.GetPosition( &Pointer, &BitsInByte );

	Status = ReadSeiPayload( PayloadType, PayloadSize );
	if( Status != FrameParserNoError )
	    return Status;

	Bits.SetPointer( Pointer + PayloadSize );
	if( Bits.Show(8) != 0x80 )
	    CheckAntiEmulationBuffer( min(DEFAULT_ANTI_EMULATION_REQUEST, (UnitLength - ConsumedSize)) );

    } while( Bits.Show(8) != 0x80 );

    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - a supplemental enhancement record
//

FrameParserStatus_t   FrameParser_VideoH264_c::ReadPlayer2ContainerParameters( void )
{
unsigned char	ParametersVersion;
unsigned int	FieldsPresentMask;
unsigned int	PixelAspectRatioNumerator;
unsigned int	PixelAspectRatioDenominator;
unsigned int	TimeScale;
unsigned int	TimeDelta;

//

    ParametersVersion	= Bits.Get(8);
    MarkerBits( 8, 0xff );

    switch( ParametersVersion )
    {
	case 0x01:
		FieldsPresentMask	= Bits.Get(16);
		MarkerBits( 8, 0xff );

		if( (FieldsPresentMask & 0x0002) != 0 )
		{
		    PixelAspectRatioNumerator	 = Bits.Get(16) << 16;
		    MarkerBits( 8, 0xff );
		    PixelAspectRatioNumerator	|= Bits.Get(16);
		    MarkerBits( 8, 0xff );

		    PixelAspectRatioDenominator	 = Bits.Get(16) << 16;
		    MarkerBits( 8, 0xff );
		    PixelAspectRatioDenominator	|= Bits.Get(16);
		    MarkerBits( 8, 0xff );

		    DefaultPixelAspectRatio	= Rational_t( PixelAspectRatioNumerator, PixelAspectRatioDenominator );
		}

 		//
		// Check if the default frame rate is present
		//

		if( (FieldsPresentMask & 0x0001) == 0 )
		    break;

	case 0x00:

		TimeScale		 = Bits.Get(16) << 16;
		MarkerBits( 8, 0xff );
		TimeScale		|= Bits.Get(16);
		MarkerBits( 8, 0xff );

		TimeDelta		 = Bits.Get(16) << 16;
		MarkerBits( 8, 0xff );
		TimeDelta		|= Bits.Get(16);
		MarkerBits( 8, 0xff );

		DefaultFrameRate		= Rational_t( TimeScale, TimeDelta ); 
		UserSpecifiedDefaultFrameRate	= true;
		break;

	default:
		report( severity_error, "FrameParser_VideoH264_c::ReadPlayer2ContainerParameters - Unsupported version of the container parameters record (%02x).\n", ParametersVersion );
		break;
    }

//

#ifdef DUMP_HEADERS
    report( severity_info, "    Player 2 container parameter message :-\n" );

    report( severity_info, "\tTimeScale                                  = %6d\n", TimeScale );
    report( severity_info, "\tTimeDelta                                  = %6d\n", TimeDelta );
#endif

//

    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - determine the pic order counts (using the algorithms in section 8.2 of the standard to get
//                the FieldOrderCnts) - Transcribed from the standard, its largely gibberish to me.
//

FrameParserStatus_t   FrameParser_VideoH264_c::CalculatePicOrderCnts(  void )
{
unsigned int    i;
bool            MemoryControlClearSeen;
unsigned int	MaxFrameNum;
unsigned int    TopFieldOrderCnt;
unsigned int    BottomFieldOrderCnt;
unsigned int    MaxPicOrderCntLsb;              // Used in type 0 calculation
int		PicOrderCntMsb;
unsigned int	FrameNumOffset;			// Used in type 1 calculation
unsigned int    AbsFrameNum;
unsigned int    PicOrderCntCycleCnt;
unsigned int    FrameNumInPicOrderCntCycle;
unsigned int    ExpectedDeltaPerPicOrderCntCycle;
unsigned int    ExpectedPicOrderCnt;
unsigned int    TempPicOrderCnt;                // Used in type 2 calculation

//

    TopFieldOrderCnt    = 9999;
    BottomFieldOrderCnt = 9999;

    MaxFrameNum         = 1 << (SliceHeader->SequenceParameterSet->log2_max_frame_num_minus4 + 4);

    MemoryControlClearSeen      = false;
    if( (nal_ref_idc != 0) && SliceHeader->dec_ref_pic_marking.adaptive_ref_pic_marking_mode_flag )
	for( i=0; SliceHeader->dec_ref_pic_marking.memory_management_control_operation[i] != H264_MMC_END; i++ )
	    if( SliceHeader->dec_ref_pic_marking.memory_management_control_operation[i] == H264_MMC_CLEAR )
		MemoryControlClearSeen = true;

//


    switch( SliceHeader->SequenceParameterSet->pic_order_cnt_type )
    {
	case 0:
		//
		// Obtain the previous PicOrderCnt values
		//

		SliceHeader->EntryPicOrderCntMsb		= PrevPicOrderCntMsb;

		if( nal_unit_type == NALU_TYPE_IDR )
		{
		    PrevPicOrderCntMsb  			= 0;
		    PrevPicOrderCntLsb  			= 0;
		    SliceHeader->ExitPicOrderCntMsbForced	= 1;
		}

		//
		// Calculate the PicOrderCntMsb
		//

		MaxPicOrderCntLsb       = 1 << (SliceHeader->SequenceParameterSet->log2_max_pic_order_cnt_lsb_minus4 + 4);

		if( (SliceHeader->pic_order_cnt_lsb < PrevPicOrderCntLsb) &&
		    ((PrevPicOrderCntLsb - SliceHeader->pic_order_cnt_lsb) >= (MaxPicOrderCntLsb/2)) )
		   PicOrderCntMsb = PrevPicOrderCntMsb + MaxPicOrderCntLsb;
		else if( (SliceHeader->pic_order_cnt_lsb > PrevPicOrderCntLsb) &&
		    ((SliceHeader->pic_order_cnt_lsb - PrevPicOrderCntLsb) > (MaxPicOrderCntLsb/2)) )
		   PicOrderCntMsb = PrevPicOrderCntMsb - MaxPicOrderCntLsb;
		else
		   PicOrderCntMsb = PrevPicOrderCntMsb;

		//
		// And calculate the appropriate field order counts
		//

		if( !(SliceHeader->field_pic_flag && SliceHeader->bottom_field_flag) )
		    TopFieldOrderCnt    = PicOrderCntMsb + SliceHeader->pic_order_cnt_lsb;

		if( !SliceHeader->field_pic_flag )
		    BottomFieldOrderCnt = TopFieldOrderCnt + SliceHeader->delta_pic_order_cnt_bottom;
		else if( SliceHeader->bottom_field_flag )
		    BottomFieldOrderCnt = PicOrderCntMsb + SliceHeader->pic_order_cnt_lsb;

		//
		// Update the PicOrderCnt values for use on the next frame
		//

		if( nal_ref_idc != 0 )		// Ref frame
		{
		    if( MemoryControlClearSeen )
		    {
		    	PrevPicOrderCntMsb			= 0;
		    	PrevPicOrderCntLsb			= (SliceHeader->field_pic_flag && SliceHeader->bottom_field_flag) ? 0 : TopFieldOrderCnt;
			SliceHeader->ExitPicOrderCntMsbForced	= 1;
		    }
		    else
		    {
			PrevPicOrderCntMsb      		= PicOrderCntMsb;
			PrevPicOrderCntLsb      		= SliceHeader->pic_order_cnt_lsb;
		    }

		    LastExitPicOrderCntMsb			= PicOrderCntMsb;
		    SliceHeader->ExitPicOrderCntMsb		= PicOrderCntMsb;
		}

		break;

// ---

	case 1:

		//
		// Calculate FrameNumOffset
		//

		if( nal_unit_type == NALU_TYPE_IDR )
		    FrameNumOffset      = 0;
		else if( PrevFrameNum > SliceHeader->frame_num )
		    FrameNumOffset      = PrevFrameNumOffset + MaxFrameNum;
		else
		    FrameNumOffset      = PrevFrameNumOffset;

		//
		// Calculate AbsFrameNum
		//

		if( SliceHeader->SequenceParameterSet->num_ref_frames_in_pic_order_cnt_cycle != 0 )
		    AbsFrameNum         = FrameNumOffset + SliceHeader->frame_num;
		else
		    AbsFrameNum         = 0;

		if( (nal_ref_idc == 0) && (AbsFrameNum > 0) )
		    AbsFrameNum--;

		//
		// If AbsFrameNum > 0 determine PicOrderCntCycle values
		//

		PicOrderCntCycleCnt             = 0;    // Initialize to remove compiler warning
		FrameNumInPicOrderCntCycle      = 0;    // Actually only used if they are initialized

		if( AbsFrameNum > 0 )
		{
		    PicOrderCntCycleCnt         = (AbsFrameNum - 1) / SliceHeader->SequenceParameterSet->num_ref_frames_in_pic_order_cnt_cycle;
		    FrameNumInPicOrderCntCycle  = (AbsFrameNum - 1) % SliceHeader->SequenceParameterSet->num_ref_frames_in_pic_order_cnt_cycle;
		}

		//
		// Calculate ExpectedDeltaPerPicOrderCntCycle
		//

		ExpectedDeltaPerPicOrderCntCycle         = 0;
		for( i=0; i<SliceHeader->SequenceParameterSet->num_ref_frames_in_pic_order_cnt_cycle; i++ )
		    ExpectedDeltaPerPicOrderCntCycle    += SliceHeader->SequenceParameterSet->offset_for_ref_frame[i];

		//
		// Calculate ExpectedPicOrderCnt
		//

		if( AbsFrameNum > 0 )
		{
		    ExpectedPicOrderCnt          = PicOrderCntCycleCnt * ExpectedDeltaPerPicOrderCntCycle;
		    for( i=0; i<=FrameNumInPicOrderCntCycle; i++ )
			ExpectedPicOrderCnt     += SliceHeader->SequenceParameterSet->offset_for_ref_frame[i];
		}
		else
		{
		    ExpectedPicOrderCnt          = 0;
		}

		if( nal_ref_idc == 0 )
		    ExpectedPicOrderCnt         += SliceHeader->SequenceParameterSet->offset_for_non_ref_pic;

		//
		// And calculate the appropriate field order counts
		//

		if( !SliceHeader->field_pic_flag )
		{
		    TopFieldOrderCnt    = ExpectedPicOrderCnt + SliceHeader->delta_pic_order_cnt[0];
		    BottomFieldOrderCnt = TopFieldOrderCnt + SliceHeader->SequenceParameterSet->offset_for_top_to_bottom_field + SliceHeader->delta_pic_order_cnt[1];
		}
		else if( !SliceHeader->bottom_field_flag )
		{
		    TopFieldOrderCnt    = ExpectedPicOrderCnt + SliceHeader->delta_pic_order_cnt[0];
		}
		else
		{
		    BottomFieldOrderCnt = ExpectedPicOrderCnt + SliceHeader->SequenceParameterSet->offset_for_top_to_bottom_field + SliceHeader->delta_pic_order_cnt[0];
		}

		//
		// Update the FrameNum values for use on the next frame
		//

		if( MemoryControlClearSeen )
		{
		    PrevFrameNumOffset		= 0;
		}
		else
		{
		    PrevFrameNumOffset		= FrameNumOffset;
		}

		PrevFrameNum			= SliceHeader->frame_num;

		break;

// ---

	case 2:
		// Note (from standard) picture order count type 2 results in an output order identical to the decode order.

		//
		// Calculate FrameNumOffset
		//

		if( nal_unit_type == NALU_TYPE_IDR )
		    FrameNumOffset      = 0;
		else if( PrevFrameNum > SliceHeader->frame_num )
		    FrameNumOffset      = PrevFrameNumOffset + MaxFrameNum;
		else
		    FrameNumOffset      = PrevFrameNumOffset;

		//
		// Calculate TempPicOrderCnt
		//

		if( nal_unit_type == NALU_TYPE_IDR )
		    TempPicOrderCnt     = 0;
		else if( nal_ref_idc == 0 )
		    TempPicOrderCnt     = 2 * (FrameNumOffset + SliceHeader->frame_num) - 1;
		else
		    TempPicOrderCnt     = 2 * (FrameNumOffset + SliceHeader->frame_num);

		//
		// And calculate the appropriate field order counts
		//

		if( !SliceHeader->field_pic_flag )
		{
		    TopFieldOrderCnt    = TempPicOrderCnt;
		    BottomFieldOrderCnt = TempPicOrderCnt;
		}
		else if( SliceHeader->bottom_field_flag )
		{
		    BottomFieldOrderCnt = TempPicOrderCnt;
		}
		else
		{
		    TopFieldOrderCnt    = TempPicOrderCnt;
		}

		//
		// Update the FrameNum values for use on the next frame
		//

		if( MemoryControlClearSeen )
		{
		    PrevFrameNumOffset		= 0;
		}
		else
		{
		    PrevFrameNumOffset		= FrameNumOffset;
		}

		PrevFrameNum			= SliceHeader->frame_num;
		break;

	default:
		break;
    }

//

    SliceHeader->PicOrderCntTop  	= TopFieldOrderCnt;
    SliceHeader->PicOrderCntBot  	= BottomFieldOrderCnt;
    SliceHeader->PicOrderCnt		= SliceHeader->field_pic_flag ?
						(SliceHeader->bottom_field_flag ? BottomFieldOrderCnt : TopFieldOrderCnt) :
						min(TopFieldOrderCnt, BottomFieldOrderCnt);

    //
    // Now calculate the extended pic order count used for frame re-ordering
    // this is based on pic order count, or on the dpb output delay if picture 
    // timing messages are present.
    //

    if( nal_unit_type == NALU_TYPE_IDR  )
    {
	PicOrderCntOffset		= PicOrderCntOffset + PicOrderCntOffsetAdjust;	// Add 2^32 to cater for negative pic order counts in the next sequence.
	BaseDpbValue			= 0;
    }

    SliceHeader->ExtendedPicOrderCnt	= DisplayOrderByDpbValues ?
				(PicOrderCntOffset + BaseDpbValue + SEIPictureTiming.dpb_output_delay) :
				(PicOrderCntOffset + SliceHeader->PicOrderCnt);

    //
    // If we have a memory control clear, and we are deriving frame 
    // re-ordering from picture order counts then increment the PicOrderCntOffset
    //

    if( MemoryControlClearSeen && !DisplayOrderByDpbValues )
    {
	PicOrderCntOffset		= PicOrderCntOffset + PicOrderCntOffsetAdjust;	// Add 2^32 to cater for negative pic order counts in the next sequence.
    }

//

#ifdef DUMP_REFLISTS
    report( severity_info, "xxx CalculatePicOrderCnts %d - (%3d %d %4d) - P %4d, T %4d, B %4d (%016llx)\n", 
		SliceHeader->SequenceParameterSet->pic_order_cnt_type,
		SliceHeader->frame_num, SliceHeader->slice_type, SliceHeader->pic_order_cnt_lsb, 
		SliceHeader->PicOrderCnt, TopFieldOrderCnt, BottomFieldOrderCnt, SliceHeader->ExtendedPicOrderCnt );
#endif
    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Perform the "Initialization process for the reference picture list for P and SP slices in frames"
//

#define BUBBLE_UP( L, F, O, N )                                                 \
	if( (N) > 1 )                                                           \
	{                                                                       \
	    unsigned int        i,j;                                            \
										\
	    for( i=1; i<(N); i++ )                                              \
		for( j=0; j<((N)-i); j++ )                                      \
		    if( ReferenceFrames[L[(O)+j]].F > ReferenceFrames[L[(O)+j+1]].F )   \
		    {                                                           \
			unsigned int    Tmp;                                    \
										\
			Tmp             = L[(O)+j];                             \
			L[(O)+j]        = L[(O)+j+1];                           \
			L[(O)+j+1]      = Tmp;                                  \
		    }                                                           \
	}

#define BUBBLE_DOWN( L, F, O, N )                                                       \
	if( (N) > 1 )                                                           \
	{                                                                       \
	    unsigned int        i,j;                                            \
										\
	    for( i=1; i<(N); i++ )                                              \
		for( j=0; j<((N)-i); j++ )                                      \
		    if( ReferenceFrames[L[(O)+j]].F < ReferenceFrames[L[(O)+j+1]].F )   \
		    {                                                           \
			unsigned int    Tmp;                                    \
										\
			Tmp             = L[(O)+j];                             \
			L[(O)+j]        = L[(O)+j+1];                           \
			L[(O)+j+1]      = Tmp;                                  \
		    }                                                           \
	}

FrameParserStatus_t   FrameParser_VideoH264_c::InitializePSliceReferencePictureListFrame( void )
{
unsigned int   		 i;
unsigned int   		 Count;
unsigned int   		 NumActiveReferences;
ReferenceFrameList_t	*List;

    //
    // Calculate the limit on active indices
    //

#ifdef REFERENCE_PICTURE_LIST_PROCESSING
    NumActiveReferences         = SliceHeader->PictureParameterSet->num_ref_idx_l0_active_minus1 + 1;
    if( SliceHeader->num_ref_idx_active_override_flag )
	NumActiveReferences     = SliceHeader->num_ref_idx_l0_active_minus1 + 1;
#else
    NumActiveReferences         = NumReferenceFrames;
#endif

    //
    // Obtain an descending list of PicNums (this is, no doubt,
    // not the fastest ordering method but given the size of
    // the lists (4 ish) it is probably not a problem).
    //

    List	= &ReferenceFrameList[P_REF_PIC_LIST];

    if( NumShortTerm != 0 )
    {
	Count   = 0;
	for( i=0; i<(NumReferenceFrames+1); i++ )
	    if( ComplimentaryReferencePair(ReferenceFrames[i].Usage) && 
		((ReferenceFrames[i].Usage & AnyUsedForShortTermReference) != 0) )
	    {
		List->EntryIndicies[Count]				= i;
		List->H264ReferenceDetails[Count].LongTermReference	= false;
		Count++;
	    }

	BUBBLE_DOWN( List->EntryIndicies, PicNum, 0, Count );

	List->EntryCount	= Count;
    }

    //
    // Now add in the long term indices
    //

    if( NumLongTerm != 0 )
    {
	Count   = 0;
	for( i=0; i<(NumReferenceFrames+1); i++ )
	    if( ComplimentaryReferencePair(ReferenceFrames[i].Usage) && 
		((ReferenceFrames[i].Usage & AnyUsedForLongTermReference) != 0) )
	    {
		List->EntryIndicies[List->EntryCount + Count]				= i;
		List->H264ReferenceDetails[List->EntryCount + Count].LongTermReference	= true;
		Count++;
	    }

	BUBBLE_UP( List->EntryIndicies, LongTermPicNum, List->EntryCount, Count );

	List->EntryCount	= min( List->EntryCount + Count, NumActiveReferences );
    }

//

    return FrameParserNoError;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Perform the "Initialization process for the reference picture list for P and SP slices in fields"
//

FrameParserStatus_t   FrameParser_VideoH264_c::InitializePSliceReferencePictureListField( void )
{
unsigned int    NumActiveReferences;
unsigned int    Count;
unsigned int    i;

    //
    // Calculate the limit on active indices
    //

#ifdef REFERENCE_PICTURE_LIST_PROCESSING
    NumActiveReferences         = SliceHeader->PictureParameterSet->num_ref_idx_l0_active_minus1 + 1;
    if( SliceHeader->num_ref_idx_active_override_flag )
	NumActiveReferences     = SliceHeader->num_ref_idx_l0_active_minus1 + 1;
#else
    NumActiveReferences         = 2 * NumReferenceFrames + 1;
#endif

    //
    // Obtain a descending list of PicNums (this is, no doubt,
    // not the fastest ordering method but given the size of
    // the lists (4 ish) it is probably not a problem).
    //

    ReferenceFrameListShortTerm[0].EntryCount	= 0;
    ReferenceFrameListLongTerm.EntryCount	= 0;

    if( NumShortTerm != 0 )
    {
	Count   = 0;
	for( i=0; i<(NumReferenceFrames+1); i++ )
	    if( (ReferenceFrames[i].Usage & AnyUsedForShortTermReference) != 0 )
		ReferenceFrameListShortTerm[0].EntryIndicies[Count++] 	= i;

	BUBBLE_DOWN( ReferenceFrameListShortTerm[0].EntryIndicies, FrameNumWrap, 0, Count );

	ReferenceFrameListShortTerm[0].EntryCount = Count;
    }

    //
    // Now add in the long term indices
    //

    if( NumLongTerm != 0 )
    {
	Count   = 0;
	for( i=0; i<(NumReferenceFrames+1); i++ )
	    if( (ReferenceFrames[i].Usage & AnyUsedForLongTermReference) != 0 )
		ReferenceFrameListLongTerm.EntryIndicies[Count++]   = i;

	BUBBLE_UP( ReferenceFrameListLongTerm.EntryIndicies, LongTermPicNum, 0, Count );

	ReferenceFrameListLongTerm.EntryCount = Count;
    }

    //
    // Finally process these to generate ReferenceFrameList 0
    //

    InitializeReferencePictureListField( &ReferenceFrameListShortTerm[0],
					 &ReferenceFrameListLongTerm,
					 NumActiveReferences,
					 &ReferenceFrameList[P_REF_PIC_LIST] );

//

    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Perform the "Initialization process for the reference picture list for B slices in frames"
//

FrameParserStatus_t   FrameParser_VideoH264_c::InitializeBSliceReferencePictureListsFrame( void )
{
unsigned int		 i;
unsigned int		 Count;
unsigned int		 LowerEntries;
unsigned int		 NumActiveReferences0;
unsigned int		 NumActiveReferences1;
ReferenceFrameList_t	*List0;
ReferenceFrameList_t	*List1;

    //
    // Calculate the limit on active indices
    //

#ifdef REFERENCE_PICTURE_LIST_PROCESSING
    NumActiveReferences0        = SliceHeader->PictureParameterSet->num_ref_idx_l0_active_minus1 + 1;
    if( SliceHeader->num_ref_idx_active_override_flag )
	NumActiveReferences0    = SliceHeader->num_ref_idx_l0_active_minus1 + 1;
#else
    NumActiveReferences0        = NumReferenceFrames;
#endif

//

#ifdef REFERENCE_PICTURE_LIST_PROCESSING
    NumActiveReferences1        = SliceHeader->PictureParameterSet->num_ref_idx_l1_active_minus1 + 1;
    if( SliceHeader->num_ref_idx_active_override_flag )
	NumActiveReferences1    = SliceHeader->num_ref_idx_l1_active_minus1 + 1;
#else
    NumActiveReferences1        = NumReferenceFrames;
#endif

    //
    // First we construct list 0, and derive list 1 from it
    //
    // Obtain descending list of short term reference
    // frames with lower picnum than the current frame,
    // and the ascending list of frames with higher
    // picnum than the current frame.

    List0	= &ReferenceFrameList[B_REF_PIC_LIST_0];
    List1	= &ReferenceFrameList[B_REF_PIC_LIST_1];

    if( NumShortTerm != 0 )
    {
	Count   = 0;
	for( i=0; i<(NumReferenceFrames+1); i++ )
	    if( ComplimentaryReferencePair(ReferenceFrames[i].Usage) && 
		((ReferenceFrames[i].Usage & AnyUsedForShortTermReference) != 0) &&
		(ReferenceFrames[i].PicOrderCnt  <= SliceHeader->PicOrderCnt) )
	    {
		List0->EntryIndicies[Count]				= i;
		List0->H264ReferenceDetails[Count].LongTermReference	= false;
		Count++;
	    }

	BUBBLE_DOWN( List0->EntryIndicies, PicOrderCnt, 0, Count );

	LowerEntries            = Count;

	Count   = 0;
	for( i=0; i<(NumReferenceFrames+1); i++ )
	    if( ComplimentaryReferencePair(ReferenceFrames[i].Usage) && 
		((ReferenceFrames[i].Usage & AnyUsedForShortTermReference) != 0) &&
		(ReferenceFrames[i].PicOrderCnt  > SliceHeader->PicOrderCnt) )
	    {
		List0->EntryIndicies[LowerEntries + Count]				= i;
		List0->H264ReferenceDetails[LowerEntries + Count].LongTermReference	= false;
		Count++;
	    }

	BUBBLE_UP( List0->EntryIndicies, PicOrderCnt, LowerEntries, Count );

	List0->EntryCount = LowerEntries + Count;

	//
	// Now copy this list (in two sepearate portions),
	// into reference list 1.
	//

	for( i=0; i<Count; i++ )
	{
	    List1->EntryIndicies[i]					= List0->EntryIndicies[LowerEntries + i];
	    List1->H264ReferenceDetails[i].LongTermReference		= false; 
	}

	for( i=0; i<LowerEntries; i++ )
	{
	    List1->EntryIndicies[Count+i]				= List0->EntryIndicies[i];
	    List1->H264ReferenceDetails[Count+i].LongTermReference	= false;
	}
    }

    //
    // Now append the long term references to list 0, and copy them to list 1
    //

    if( NumLongTerm != 0 )
    {
	Count   = 0;
	for( i=0; i<(NumReferenceFrames+1); i++ )
	    if( ComplimentaryReferencePair(ReferenceFrames[i].Usage) && 
		((ReferenceFrames[i].Usage & AnyUsedForLongTermReference) != 0) )
	    {
		List0->EntryIndicies[List0->EntryCount + Count]					= i;
		List0->H264ReferenceDetails[List0->EntryCount + Count].LongTermReference	= true;
		Count++;
	    }

	BUBBLE_UP( List0->EntryIndicies, LongTermPicNum, List0->EntryCount, Count );

	for( i=0; i<Count; i++ )
	{
	    List1->EntryIndicies[List0->EntryCount+i]  				= List0->EntryIndicies[List0->EntryCount+i];
	    List1->H264ReferenceDetails[List0->EntryCount+i].LongTermReference	= true;
	}

	List0->EntryCount += Count;
    }

    //
    // Now limit the length of both lists as specified by the active flags
    //

    List1->EntryCount	= min( List0->EntryCount, NumActiveReferences1 );
    List0->EntryCount	= min( List0->EntryCount, NumActiveReferences0 );

    //
    // Finally, if the lists are identical, and have more than
    // 1 entry, then we swap the first two entries of list 1.
    //

    if( (List1->EntryCount > 1) &&
	(List1->EntryCount == List0->EntryCount) &&
	(memcmp(List0->EntryIndicies, List1->EntryIndicies, List1->EntryCount * sizeof(unsigned int)) == 0) )
    {
	unsigned int    Tmp;
	unsigned int    TmpBool;

	Tmp			= List1->EntryIndicies[0];
	List1->EntryIndicies[0]	= List1->EntryIndicies[1];
	List1->EntryIndicies[1]	= Tmp;

	TmpBool							= List1->H264ReferenceDetails[0].LongTermReference;
	List1->H264ReferenceDetails[0].LongTermReference	= List1->H264ReferenceDetails[1].LongTermReference;
	List1->H264ReferenceDetails[1].LongTermReference	= TmpBool;
    }

//

    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Perform the "Initialization process for the reference picture list for B slices in fields"
//

FrameParserStatus_t   FrameParser_VideoH264_c::InitializeBSliceReferencePictureListsField( void )
{
unsigned int    i;
unsigned int    Count;
unsigned int    LowerEntries;
unsigned int    NumActiveReferences0;
unsigned int    NumActiveReferences1;

    //
    // Calculate the limit on active indices
    //

#ifdef REFERENCE_PICTURE_LIST_PROCESSING
    NumActiveReferences0        = SliceHeader->PictureParameterSet->num_ref_idx_l0_active_minus1 + 1;
    if( SliceHeader->num_ref_idx_active_override_flag )
	NumActiveReferences0    = SliceHeader->num_ref_idx_l0_active_minus1 + 1;
#else
    NumActiveReferences0        = 2 * NumReferenceFrames + 1;
#endif

//

#ifdef REFERENCE_PICTURE_LIST_PROCESSING
    NumActiveReferences1        = SliceHeader->PictureParameterSet->num_ref_idx_l1_active_minus1 + 1;
    if( SliceHeader->num_ref_idx_active_override_flag )
	NumActiveReferences1    = SliceHeader->num_ref_idx_l1_active_minus1 + 1;
#else
    NumActiveReferences1        = 2 * NumReferenceFrames + 1;
#endif

    //
    // First we construct list 0, and derive list 1 from it
    //
    // Obtain descending list of short term reference
    // frames with lower picnum than the current frame,
    // and the ascending list of frames with higher
    // picnum than the current frame.

    ReferenceFrameListShortTerm[0].EntryCount	= 0;
    ReferenceFrameListShortTerm[1].EntryCount	= 0;
    ReferenceFrameListLongTerm.EntryCount	= 0;

    LowerEntries				= 0;
    Count					= 0;

    if( NumShortTerm != 0 )
    {
	Count   = 0;
	for( i=0; i<(NumReferenceFrames+1); i++ )
	    if( ((ReferenceFrames[i].Usage & AnyUsedForShortTermReference) != 0) &&
		(ReferenceFrames[i].PicOrderCnt <= SliceHeader->PicOrderCnt) )
		ReferenceFrameListShortTerm[0].EntryIndicies[Count++] = i;

	BUBBLE_DOWN( ReferenceFrameListShortTerm[0].EntryIndicies, PicOrderCnt, 0, Count );

	LowerEntries            = Count;

	Count   = 0;
	for( i=0; i<(NumReferenceFrames+1); i++ )
	    if( ((ReferenceFrames[i].Usage & AnyUsedForShortTermReference) != 0) &&
		(ReferenceFrames[i].PicOrderCnt  > SliceHeader->PicOrderCnt) )
		ReferenceFrameListShortTerm[0].EntryIndicies[LowerEntries + Count++]  = i;

	BUBBLE_UP( ReferenceFrameListShortTerm[0].EntryIndicies, PicOrderCnt, LowerEntries, Count );

	ReferenceFrameListShortTerm[0].EntryCount = LowerEntries + Count;

	//
	// Now copy this list (in two sepearate portions),
	// into reference list 1.
	//

	for( i=0; i<Count; i++ )
	    ReferenceFrameListShortTerm[1].EntryIndicies[i]		=  ReferenceFrameListShortTerm[0].EntryIndicies[LowerEntries + i];

	for( i=0; i<LowerEntries; i++ )
	    ReferenceFrameListShortTerm[1].EntryIndicies[Count+i]	=  ReferenceFrameListShortTerm[0].EntryIndicies[i];

	ReferenceFrameListShortTerm[1].EntryCount 			= ReferenceFrameListShortTerm[0].EntryCount;
    }

    //
    // Now generate the long term references
    //

    if( NumLongTerm != 0 )
    {
	Count   = 0;
	for( i=0; i<(NumReferenceFrames+1); i++ )
	    if( (ReferenceFrames[i].Usage & AnyUsedForLongTermReference) != 0 )
		ReferenceFrameListLongTerm.EntryIndicies[Count++]   = i;

	BUBBLE_UP( ReferenceFrameListLongTerm.EntryIndicies, LongTermPicNum, 0, Count );

	ReferenceFrameListLongTerm.EntryCount = Count;
    }

    //
    // Dump the lists for posterity
    //

#ifdef DUMP_REFLISTS
    report( severity_info, "    Reference frame lists (%s) PicOrderCnt = %d :-\n",
	(!SliceHeader->bottom_field_flag ? "Top field" : "Bottom field"), SliceHeader->PicOrderCnt );

    report( severity_info, "    ReferenceFrameListShortTerm[0].EntryCount - %d entries (Lower %d Upper %d):-\n", ReferenceFrameListShortTerm[0].EntryCount, LowerEntries, Count );
    for( i=0; i<ReferenceFrameListShortTerm[0].EntryCount; i++ )
    {
	report( severity_info, "\tDecodeIndex %5d, FrameNumber %4d PicOrderCnt %4d (%4d)\n",
		ReferenceFrames[ReferenceFrameListShortTerm[0].EntryIndicies[i]].DecodeFrameIndex,
		ReferenceFrames[ReferenceFrameListShortTerm[0].EntryIndicies[i]].FrameNum,
		ReferenceFrames[ReferenceFrameListShortTerm[0].EntryIndicies[i]].PicOrderCntTop, ReferenceFrames[ReferenceFrameListShortTerm[0].EntryIndicies[i]].PicOrderCntBot );
    }

    report( severity_info, "    ReferenceFrameListShortTerm[1].EntryCount - %d entries (Lower %d Upper %d:-\n", ReferenceFrameListShortTerm[1].EntryCount, LowerEntries, Count );
    for( i=0; i<ReferenceFrameListShortTerm[1].EntryCount; i++ )
    {
	report( severity_info, "\tDecodeIndex %5d, FrameNumber %4d PicOrderCnt %4d (%4d)\n",
		ReferenceFrames[ReferenceFrameListShortTerm[1].EntryIndicies[i]].DecodeFrameIndex,
		ReferenceFrames[ReferenceFrameListShortTerm[1].EntryIndicies[i]].FrameNum,
		ReferenceFrames[ReferenceFrameListShortTerm[1].EntryIndicies[i]].PicOrderCntTop, ReferenceFrames[ReferenceFrameListShortTerm[1].EntryIndicies[i]].PicOrderCntBot );
    }

    report( severity_info, "    ReferenceFrameListLongTerm.EntryCount - %d entries (Count %d):-\n", ReferenceFrameListLongTerm.EntryCount, Count );
    for( i=0; i<ReferenceFrameListLongTerm.EntryCount; i++ )
    {
	report( severity_info, "\tDecodeIndex %5d, FrameNumber %4d PicOrderCnt %4d (%4d)\n",
		ReferenceFrames[ReferenceFrameListLongTerm.EntryIndicies[i]].DecodeFrameIndex,
		ReferenceFrames[ReferenceFrameListLongTerm.EntryIndicies[i]].FrameNum,
		ReferenceFrames[ReferenceFrameListLongTerm.EntryIndicies[i]].PicOrderCntTop, ReferenceFrames[ReferenceFrameListLongTerm.EntryIndicies[i]].PicOrderCntBot );
    }
#endif

    //
    // Process these to generate RefPicLists
    //

    InitializeReferencePictureListField( &ReferenceFrameListShortTerm[0],
					 &ReferenceFrameListLongTerm,
					 NumActiveReferences0,
					 &ReferenceFrameList[B_REF_PIC_LIST_0] );

    InitializeReferencePictureListField( &ReferenceFrameListShortTerm[1],
					 &ReferenceFrameListLongTerm,
					 NumActiveReferences1,
					 &ReferenceFrameList[B_REF_PIC_LIST_1] );

    //
    // Finally, if the lists are identical, and have more than
    // 1 entry, then we swap the first two entries of list 1.
    //

    if( (ReferenceFrameList[B_REF_PIC_LIST_1].EntryCount > 1) &&
	(ReferenceFrameList[B_REF_PIC_LIST_1].EntryCount == ReferenceFrameList[B_REF_PIC_LIST_0].EntryCount) &&
	(memcmp(ReferenceFrameList[B_REF_PIC_LIST_0].EntryIndicies, ReferenceFrameList[B_REF_PIC_LIST_1].EntryIndicies, ReferenceFrameList[B_REF_PIC_LIST_1].EntryCount * sizeof(unsigned int)) == 0) )
    {
	unsigned int    Tmp;
	unsigned int    TmpBool;

	Tmp             							= ReferenceFrameList[B_REF_PIC_LIST_1].EntryIndicies[0];
	ReferenceFrameList[B_REF_PIC_LIST_1].EntryIndicies[0]			= ReferenceFrameList[B_REF_PIC_LIST_1].EntryIndicies[1];
	ReferenceFrameList[B_REF_PIC_LIST_1].EntryIndicies[1] 			= Tmp;

	Tmp             							= ReferenceFrameList[B_REF_PIC_LIST_1].H264ReferenceDetails[0].UsageCode;
	ReferenceFrameList[B_REF_PIC_LIST_1].H264ReferenceDetails[0].UsageCode	= ReferenceFrameList[B_REF_PIC_LIST_1].H264ReferenceDetails[1].UsageCode;
	ReferenceFrameList[B_REF_PIC_LIST_1].H264ReferenceDetails[1].UsageCode  = Tmp;

	TmpBool             								= ReferenceFrameList[B_REF_PIC_LIST_1].H264ReferenceDetails[0].LongTermReference;
	ReferenceFrameList[B_REF_PIC_LIST_1].H264ReferenceDetails[0].LongTermReference	= ReferenceFrameList[B_REF_PIC_LIST_1].H264ReferenceDetails[1].LongTermReference;
	ReferenceFrameList[B_REF_PIC_LIST_1].H264ReferenceDetails[1].LongTermReference  = TmpBool;
    }

//

    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Perform the "Initialization process for reference picture lists in fields" in the specification
//

FrameParserStatus_t   FrameParser_VideoH264_c::InitializeReferencePictureListField(
								ReferenceFrameList_t	*ShortTermList,
								ReferenceFrameList_t	*LongTermList,
								unsigned int		 MaxListEntries,
								ReferenceFrameList_t	*List )
{
unsigned int    SameParityIndex;
unsigned int    AlternateParityIndex;
unsigned int    SameParityUsedForShortTermReference;
unsigned int    AlternateParityUsedForShortTermReference;
unsigned int    SameParityUsedForLongTermReference;
unsigned int    AlternateParityUsedForLongTermReference;
unsigned int    SameParityUse;
unsigned int    AlternateParityUse;

    //
    // Calculate useful values
    //

    if( !SliceHeader->bottom_field_flag )
    {
	SameParityUsedForShortTermReference             = UsedForTopShortTermReference;
	AlternateParityUsedForShortTermReference        = UsedForBotShortTermReference;
	SameParityUsedForLongTermReference              = UsedForTopLongTermReference;
	AlternateParityUsedForLongTermReference         = UsedForBotLongTermReference;
	SameParityUse                                   = REF_PIC_USE_FIELD_TOP;
	AlternateParityUse                              = REF_PIC_USE_FIELD_BOT;
    }
    else
    {
	SameParityUsedForShortTermReference             = UsedForBotShortTermReference;
	AlternateParityUsedForShortTermReference        = UsedForTopShortTermReference;
	SameParityUsedForLongTermReference              = UsedForBotLongTermReference;
	AlternateParityUsedForLongTermReference         = UsedForTopLongTermReference;
	SameParityUse                                   = REF_PIC_USE_FIELD_BOT;
	AlternateParityUse                              = REF_PIC_USE_FIELD_TOP;
    }

    //
    // Insert the entries for the short term list
    //

    List->EntryCount	= 0;

    for( SameParityIndex = 0,           AlternateParityIndex = 0;
	 (SameParityIndex < ShortTermList->EntryCount) || (AlternateParityIndex < ShortTermList->EntryCount);
	 SameParityIndex++,             AlternateParityIndex++ )
    {
	//
	// Append next reference field of same parity in list, if any exist
	//

	while(  (SameParityIndex < ShortTermList->EntryCount) &&
		((ReferenceFrames[ShortTermList->EntryIndicies[SameParityIndex]].Usage & SameParityUsedForShortTermReference) == 0) )
	    SameParityIndex++;

	if( SameParityIndex < ShortTermList->EntryCount )
	{
	    List->EntryIndicies[List->EntryCount]				= ShortTermList->EntryIndicies[SameParityIndex];
	    List->H264ReferenceDetails[List->EntryCount].LongTermReference	= false;
	    List->H264ReferenceDetails[List->EntryCount++].UsageCode		= SameParityUse;
	}

	//
	// Append next reference field of alternate parity in list, if any exist
	//

	while(  (AlternateParityIndex < ShortTermList->EntryCount) &&
		((ReferenceFrames[ShortTermList->EntryIndicies[AlternateParityIndex]].Usage & AlternateParityUsedForShortTermReference) == 0) )
	    AlternateParityIndex++;

	if( AlternateParityIndex < ShortTermList->EntryCount )
	{
	    List->EntryIndicies[List->EntryCount]				= ShortTermList->EntryIndicies[AlternateParityIndex];
	    List->H264ReferenceDetails[List->EntryCount].LongTermReference	= false;
	    List->H264ReferenceDetails[List->EntryCount++].UsageCode		= AlternateParityUse;
	}
    }

    //
    // Insert the entries for the long term list
    //

    for( SameParityIndex = 0,           AlternateParityIndex = 0;
	 (SameParityIndex < LongTermList->EntryCount) || (AlternateParityIndex < LongTermList->EntryCount);
	 SameParityIndex++,             AlternateParityIndex++ )
    {
	//
	// Append next reference field of same parity in list, if any exist
	//

	while(  (SameParityIndex < LongTermList->EntryCount) &&
		((ReferenceFrames[LongTermList->EntryIndicies[SameParityIndex]].Usage & SameParityUsedForLongTermReference) == 0) )
	    SameParityIndex++;

	if( SameParityIndex < LongTermList->EntryCount )
	{
	    List->EntryIndicies[List->EntryCount]				= LongTermList->EntryIndicies[SameParityIndex];
	    List->H264ReferenceDetails[List->EntryCount].LongTermReference	= true;
	    List->H264ReferenceDetails[List->EntryCount++].UsageCode		= SameParityUse;
	}

	//
	// Append next reference field of alternate parity in list, if any exist
	//

	while(  (AlternateParityIndex < LongTermList->EntryCount) &&
		((ReferenceFrames[LongTermList->EntryIndicies[AlternateParityIndex]].Usage & AlternateParityUsedForLongTermReference) == 0) )
	    AlternateParityIndex++;

	if( AlternateParityIndex < LongTermList->EntryCount )
	{
	    List->EntryIndicies[List->EntryCount]				= LongTermList->EntryIndicies[AlternateParityIndex];
	    List->H264ReferenceDetails[List->EntryCount].LongTermReference	= true;
	    List->H264ReferenceDetails[List->EntryCount++].UsageCode		= AlternateParityUse;
	}
    }

    //
    // Limit the list
    //

    if( List->EntryCount > MaxListEntries )
	List->EntryCount  = MaxListEntries;

//

    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Perform the "Decoded reference picture marking process" in the specification
//

FrameParserStatus_t   FrameParser_VideoH264_c::CalculateReferencePictureListsFrame( void )
{
unsigned int    i,j;
unsigned int	MaxFrameNum;
int             FrameNumWrap;

    //
    // First we calculate Picture numbers for each reference frame
    // These can be re-used in the marking process.
    //

    MaxFrameNum         = 1 << (SliceHeader->SequenceParameterSet->log2_max_frame_num_minus4 + 4);

    for( i=0; i<(NumReferenceFrames+1); i++ )
	if( ReferenceFrames[i].Usage != NotUsedForReference )
	{
	    FrameNumWrap			= (ReferenceFrames[i].FrameNum > SliceHeader->frame_num) ?
							ReferenceFrames[i].FrameNum - MaxFrameNum :
							ReferenceFrames[i].FrameNum;

	    ReferenceFrames[i].FrameNumWrap	= FrameNumWrap;
	    ReferenceFrames[i].PicNum		= FrameNumWrap;
	    ReferenceFrames[i].LongTermPicNum	= ReferenceFrames[i].LongTermFrameIdx;
    }

    //
    // Now create the lists
    //

    ReferenceFrameList[P_REF_PIC_LIST].EntryCount	= 0;
    ReferenceFrameList[B_REF_PIC_LIST_0].EntryCount	= 0;
    ReferenceFrameList[B_REF_PIC_LIST_1].EntryCount	= 0;

    InitializePSliceReferencePictureListFrame();
    InitializeBSliceReferencePictureListsFrame();

    //
    // Fill in the data fields
    //

    for( i=0; i<H264_NUM_REF_FRAME_LISTS; i++ )
	for( j=0; j<ReferenceFrameList[i].EntryCount; j++ )
	{
	    unsigned int	I		= ReferenceFrameList[i].EntryIndicies[j];
	    bool		LongTerm	= ReferenceFrameList[i].H264ReferenceDetails[j].LongTermReference;

	    ReferenceFrameList[i].H264ReferenceDetails[j].PictureNumber		= LongTerm ? ReferenceFrames[I].LongTermPicNum : ReferenceFrames[I].PicNum;
	    ReferenceFrameList[i].H264ReferenceDetails[j].PicOrderCnt		= ReferenceFrames[I].PicOrderCnt;
	    ReferenceFrameList[i].H264ReferenceDetails[j].PicOrderCntTop	= ReferenceFrames[I].PicOrderCntTop;
	    ReferenceFrameList[i].H264ReferenceDetails[j].PicOrderCntBot	= ReferenceFrames[I].PicOrderCntBot;
	    ReferenceFrameList[i].H264ReferenceDetails[j].UsageCode		= REF_PIC_USE_FRAME;
	}

//

#ifdef REFERENCE_PICTURE_LIST_PROCESSING
	if( (SliceHeader->ref_pic_list_reordering.ref_pic_list_reordering_flag_l0 != 0) ||
	    (SliceHeader->ref_pic_list_reordering.ref_pic_list_reordering_flag_l1 != 0) )
	    report( severity_error, "FrameParser_VideoH264_c::CalculateReferenceFrameListsFrame - This implementation does not support reference picture list re-ordering.\n" );
#endif

//

    return FrameParserNoError;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - calculate reference picture lists for field decodes
//

FrameParserStatus_t   FrameParser_VideoH264_c::CalculateReferencePictureListsField( void )
{
unsigned int    i,j;
unsigned int	MaxFrameNum;
int             FrameNumWrap;

    //
    // First we calculate Picture numbers for each reference frame
    // These can be re-used in the marking process.
    //

    MaxFrameNum         = 1 << (SliceHeader->SequenceParameterSet->log2_max_frame_num_minus4 + 4);

    for( i=0; i<(NumReferenceFrames+1); i++ )
	if( ReferenceFrames[i].Usage != NotUsedForReference )
	{
	    FrameNumWrap			= (ReferenceFrames[i].FrameNum > SliceHeader->frame_num) ?
							ReferenceFrames[i].FrameNum - MaxFrameNum :
							ReferenceFrames[i].FrameNum;

	    ReferenceFrames[i].FrameNumWrap	= FrameNumWrap;
	    ReferenceFrames[i].PicNum		= (2 * FrameNumWrap);
	    ReferenceFrames[i].LongTermPicNum	= (2 * ReferenceFrames[i].LongTermFrameIdx);
    }

    //
    // Now create the lists
    //

    ReferenceFrameList[P_REF_PIC_LIST].EntryCount	= 0;
    ReferenceFrameList[B_REF_PIC_LIST_0].EntryCount	= 0;
    ReferenceFrameList[B_REF_PIC_LIST_1].EntryCount	= 0;

    InitializePSliceReferencePictureListField();
    InitializeBSliceReferencePictureListsField();

    //
    // Fill in the data fields
    //

    for( i=0; i<H264_NUM_REF_FRAME_LISTS; i++ )
	for( j=0; j<ReferenceFrameList[i].EntryCount; j++ )
	{
	    unsigned int	I		= ReferenceFrameList[i].EntryIndicies[j];
	    bool		LongTerm	= ReferenceFrameList[i].H264ReferenceDetails[j].LongTermReference;
	    bool		Top		= (ReferenceFrameList[i].H264ReferenceDetails[j].UsageCode == REF_PIC_USE_FIELD_TOP);
	    unsigned int	ParityOffset	= (Top == (SliceHeader->bottom_field_flag == 0)) ? 1 : 0;

	    ReferenceFrameList[i].H264ReferenceDetails[j].PictureNumber		= (LongTerm ? ReferenceFrames[I].LongTermPicNum : ReferenceFrames[I].PicNum) + ParityOffset;
	    ReferenceFrameList[i].H264ReferenceDetails[j].PicOrderCnt		= Top ? ReferenceFrames[I].PicOrderCntTop : ReferenceFrames[I].PicOrderCntBot;
	    ReferenceFrameList[i].H264ReferenceDetails[j].PicOrderCntTop	= ReferenceFrames[I].PicOrderCntTop;
	    ReferenceFrameList[i].H264ReferenceDetails[j].PicOrderCntBot	= ReferenceFrames[I].PicOrderCntBot;
	}

//

#ifdef REFERENCE_PICTURE_LIST_PROCESSING
	if( (SliceHeader->ref_pic_list_reordering.ref_pic_list_reordering_flag_l0 != 0) ||
	    (SliceHeader->ref_pic_list_reordering.ref_pic_list_reordering_flag_l1 != 0) )
	    report( severity_error, "FrameParser_VideoH264_c::CalculateReferenceFrameListsField - This implementation does not support reference picture list re-ordering.\n" );
#endif

//

    return FrameParserNoError;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Perform the "Decoded reference picture marking process" in the specification
//

FrameParserStatus_t   FrameParser_VideoH264_c::PrepareReferenceFrameList( void )
{
unsigned int		i,j;
FrameParserStatus_t	Status;
bool			ApplyTwoRefTestForBframes;

    //
    // Caluclate the reference picture lists
    //
 
    SliceHeader		= &((H264FrameParameters_t *)(ParsedFrameParameters->FrameParameterStructure))->SliceHeader;
    NumReferenceFrames	= SliceHeader->SequenceParameterSet->num_ref_frames;

    //
    // Perform the calculation of the lists
    //

    if( !SliceHeader->SequenceParameterSet->frame_mbs_only_flag && SliceHeader->field_pic_flag )
	Status	= CalculateReferencePictureListsField();
    else
	Status	= CalculateReferencePictureListsFrame();

    //
    // Can we identify too few reference frames - this is not optimal, as it assumes that
    // one reference is sufficient, it works for broadcast BBC, but strictly we should 
    // use IDR re-synchronization.
    //
    // specific case fails for Allegro, works for BBC+Star wars reverse, made to 
    // work for allegro by only applying B frame check during reverse play.
    //

    ApplyTwoRefTestForBframes	= !SeenAnIDR || (PlaybackDirection == PlayBackward);

    if( (Status == FrameParserNoError) &&
	((ParsedVideoParameters->SliceType != SliceTypeI) && ((NumShortTerm + NumLongTerm) < 1)) ||
	(ApplyTwoRefTestForBframes && (ParsedVideoParameters->SliceType == SliceTypeB) && ((NumShortTerm + NumLongTerm) < 2)) )
	Status	= FrameParserInsufficientReferenceFrames;

    //
    // Dump out the information relating to the reference 
    // frame lists, before translating to the indices.
    //

#ifdef DUMP_REFLISTS
{
    report( severity_info, "\n    Reference picture lists (%d %d %d) (%c slice) (%s):-\n",
	ReferenceFrameList[0].EntryCount, ReferenceFrameList[1].EntryCount, ReferenceFrameList[2].EntryCount,
	(ParsedVideoParameters->SliceType == SliceTypeB) ? 'B' :
	((ParsedVideoParameters->SliceType == SliceTypeP) ? 'P' : 'I'),
	(!SliceHeader->SequenceParameterSet->frame_mbs_only_flag && SliceHeader->field_pic_flag ? (!SliceHeader->bottom_field_flag ? "Top field" : "Bottom field") : "Frame") );

    for( i=0; i<H264_NUM_REF_FRAME_LISTS; i++ )
    {
	report( severity_info, "    ReferenceFrameList[%d] - %d entries :-\n", i, ReferenceFrameList[i].EntryCount );
	for( j=0; j<ReferenceFrameList[i].EntryCount; j++ )
	{
	    unsigned int	I		= ReferenceFrameList[i].EntryIndicies[j];

	    report( severity_info, "\tDecodeIndex %5d, Field/Frame '%s' Usage %s PictureNumber %4d PicOrderCnt T %4d B %4d X %4d\n",
			ReferenceFrames[I].DecodeFrameIndex,
			(ReferenceFrameList[i].H264ReferenceDetails[j].UsageCode ? ((ReferenceFrameList[i].H264ReferenceDetails[j].UsageCode==REF_PIC_USE_FIELD_TOP) ? "Top Field" : "Bottom Field") : "Frame"),
			(ReferenceFrameList[i].H264ReferenceDetails[j].LongTermReference ? "LongTerm, " : "ShortTerm,"),
			ReferenceFrameList[i].H264ReferenceDetails[j].PictureNumber,
			ReferenceFrameList[i].H264ReferenceDetails[j].PicOrderCntTop,
			ReferenceFrameList[i].H264ReferenceDetails[j].PicOrderCntBot,
			ReferenceFrameList[i].H264ReferenceDetails[j].PicOrderCnt );
	}
    }

    report( severity_info, "\n" );
}
#endif

    //
    // Translate the indices in the list from ReferenceFrames indices to 
    // decode frame indices as used throughout the rest of the player.
    //

    for( i=0; i<H264_NUM_REF_FRAME_LISTS; i++ )
	for( j=0; j<ReferenceFrameList[i].EntryCount; j++ )
	    ReferenceFrameList[i].EntryIndicies[j]		= ReferenceFrames[ReferenceFrameList[i].EntryIndicies[j]].DecodeFrameIndex;

    //
    // If this is not an I slice Copy the calculated reference picture 
    // list into the parsed frame parameters, otherwise leave it blank.
    //

    ParsedFrameParameters->NumberOfReferenceFrameLists	= H264_NUM_REF_FRAME_LISTS;

    if( !ParsedFrameParameters->IndependentFrame )
	memcpy( ParsedFrameParameters->ReferenceFrameList, ReferenceFrameList, H264_NUM_REF_FRAME_LISTS * sizeof(ReferenceFrameList_t) );

//

    return Status;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Release reference frame or field
//

FrameParserStatus_t   FrameParser_VideoH264_c::ReleaseReference(	bool		  ActuallyRelease,
									unsigned int	  Entry,
									unsigned int	  ReleaseUsage )
{
unsigned int	OldUsage;
unsigned int	UsedForShortTermReference;
unsigned int    UsedForLongTermReference;

//

    UsedForShortTermReference	= AnyUsedForShortTermReference & ReleaseUsage;
    UsedForLongTermReference	= AnyUsedForLongTermReference & ReleaseUsage;
    OldUsage			= ReferenceFrames[Entry].Usage;

//

    ReferenceFrames[Entry].Usage	= ReferenceFrames[Entry].Usage & (~ReleaseUsage);

#ifdef DUMP_REFLISTS
report( severity_info, "MMCO - Release %d (%d) - %04x => %04x\n", ReferenceFrames[Entry].FrameNum, Entry, ReleaseUsage, ReferenceFrames[Entry].Usage  );
#endif

    if( (ReferenceFrames[Entry].Usage == NotUsedForReference) && ActuallyRelease )
    {
	Player->CallInSequence( Stream, SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, ReferenceFrames[Entry].DecodeFrameIndex );
	ProcessDeferredDFIandPTSUpto( ReferenceFrames[Entry].ExtendedPicOrderCnt );
    }

//

    if( ((OldUsage & AnyUsedForShortTermReference) != 0) &&
	((ReferenceFrames[Entry].Usage & AnyUsedForShortTermReference) == 0) )
	NumShortTerm--;

    if( ((OldUsage & AnyUsedForLongTermReference) != 0) &&
	((ReferenceFrames[Entry].Usage & AnyUsedForLongTermReference) == 0) )
	NumLongTerm--;

//

    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Perform the "Decoded reference picture marking process" in the specification
//

FrameParserStatus_t   FrameParser_VideoH264_c::MarkReferencePictures( bool	ActuallyReleaseReferenceFrames )
{
unsigned int             i,j;
unsigned int             Entry;
unsigned int             CurrentEntry;
unsigned int		 FrameNum;
unsigned int		 MaxFrameNum;
int                      FrameNumWrap;
int                      LowestFrameNumWrap;
H264DecRefPicMarking_t  *MMC;
bool			 Top;
bool			 Field;
bool                     Idr;
bool                     LongTerm;
bool                     ProcessControl;
int			 PicNumX;
bool			 SecondFieldEntry;

//

unsigned int             UsedForShortTermReference;
unsigned int             OtherFieldUsedForShortTermReference;
unsigned int             UsedForLongTermReference;
unsigned int             OtherFieldUsedForLongTermReference;
unsigned int             FieldIndex;
unsigned int             FieldUsed;
unsigned int             OtherFieldUsed;
unsigned int		 ReleaseCode;
unsigned int		 LongTermIndexAssignedTo;

    //
    // Derive useful flags
    //

    Field		= (SliceHeader->field_pic_flag != 0);
    Top			= Field && (SliceHeader->bottom_field_flag == 0);
    Idr                 = (SliceHeader->nal_unit_type == NALU_TYPE_IDR);
    LongTerm            = (SliceHeader->dec_ref_pic_marking.long_term_reference_flag != 0);
    MMC                 = &SliceHeader->dec_ref_pic_marking;
    ProcessControl      = MMC->adaptive_ref_pic_marking_mode_flag;

    MaxFrameNum         = 1 << (SliceHeader->SequenceParameterSet->log2_max_frame_num_minus4 + 4);
    FrameNum            = SliceHeader->frame_num;

    //
    // Setup appropriate masking values
    //

    if( Field )
    {
	if( Top )
	{
	    UsedForShortTermReference           = UsedForTopShortTermReference;
	    OtherFieldUsedForShortTermReference = UsedForBotShortTermReference;
	    UsedForLongTermReference            = UsedForTopLongTermReference;
	    OtherFieldUsedForLongTermReference  = UsedForBotLongTermReference;

	    FieldIndex                          = 0;
	}
	else
	{
	    UsedForShortTermReference           = UsedForBotShortTermReference;
	    OtherFieldUsedForShortTermReference = UsedForTopShortTermReference;
	    UsedForLongTermReference            = UsedForBotLongTermReference;
	    OtherFieldUsedForLongTermReference  = UsedForTopLongTermReference;

	    FieldIndex                          = 1;
	}

	FieldUsed                       	= UsedForShortTermReference             | UsedForLongTermReference;
	OtherFieldUsed                  	= OtherFieldUsedForShortTermReference   | OtherFieldUsedForLongTermReference;
    }
    else
    {
	UsedForShortTermReference       	= UsedForTopShortTermReference | UsedForBotShortTermReference;
	UsedForLongTermReference        	= UsedForTopLongTermReference  | UsedForBotLongTermReference;
	OtherFieldUsedForShortTermReference	= 0;
	OtherFieldUsedForLongTermReference	= 0;

	FieldIndex                      	= 0;
    }

    //
    // Is this a field of a pre-existing frame
    //

    CurrentEntry        = 0;
    SecondFieldEntry	= false;

    if( Field )
    {
	for( i=0; i<(NumReferenceFrames+1); i++ )
	    if( (ReferenceFrames[i].Usage != NotUsedForReference) &&
		ReferenceFrames[i].Field &&
		(ReferenceFrames[i].FrameNum == FrameNum) &&
		((ReferenceFrames[i].DecodeFrameIndex+1) == ParsedFrameParameters->DecodeFrameIndex) )
	    {
		if( ((ReferenceFrames[i].Usage & FieldUsed)      != 0) ||
		    ((ReferenceFrames[i].Usage & OtherFieldUsed) == 0) )
		{
		    report( severity_error, "FrameParser_VideoH264_c::MarkReferencePictures - Duplicate reference field, or invalid reference list entry.\n" );
		    return FrameParserStreamSyntaxError;
		}

		if( Top )
		    ReferenceFrames[i].PicOrderCntTop			 = SliceHeader->PicOrderCntTop;
		else
		    ReferenceFrames[i].PicOrderCntBot			 = SliceHeader->PicOrderCntBot;

		ReferenceFrames[i].PicOrderCnt				 = min( ReferenceFrames[i].PicOrderCntTop, ReferenceFrames[i].PicOrderCntBot );

		if( LongTerm )
		{
		    ReferenceFrames[i].Usage                            |= UsedForLongTermReference;
		    ReferenceFrames[i].LongTermFrameIdx                  = FrameNum;

		    if( (ReferenceFrames[i].Usage & OtherFieldUsedForLongTermReference) == 0 )
			NumLongTerm++;
		}
		else
		{
		    ReferenceFrames[i].Usage                            |= UsedForShortTermReference;
		    ReferenceFrames[i].FrameNum                          = FrameNum;

		    if( (ReferenceFrames[i].Usage & OtherFieldUsedForShortTermReference) == 0 )
			NumShortTerm++;
		}

		// Release immediately, since the buffer is held by the first field
		if( ActuallyReleaseReferenceFrames )
		    Player->CallInSequence( Stream, SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, ParsedFrameParameters->DecodeFrameIndex );

		CurrentEntry		= i;
		SecondFieldEntry	= true;
	    }
    }

    //
    // If we have a non-pre-existing IDR release everyone
    //

    if( !SecondFieldEntry && Idr )
    {
	for( i=0; i<(NumReferenceFrames+1); i++ )
	    if( ReferenceFrames[i].Usage != NotUsedForReference )
	    {
		if( ActuallyReleaseReferenceFrames )
		    Player->CallInSequence( Stream, SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, ReferenceFrames[i].DecodeFrameIndex );

		ReferenceFrames[i].Usage                = NotUsedForReference;
	    }

	NumLongTerm     = 0;
	NumShortTerm    = 0;
    }

    //
    // Insert the current frame
    //

    if( !SecondFieldEntry )
	for( i=0; i<(NumReferenceFrames+1); i++ ) {
	    if (ReferenceFrames[i].Usage != NotUsedForReference &&
		ReferenceFrames[i].FrameNum == FrameNum) {
		report( severity_info, "deleting obsolete framenum %d\n", FrameNum );
		ReleaseReference( true, i, AnyUsedForReference );
	    }
	    if( ReferenceFrames[i].Usage == NotUsedForReference )
	    {
	    	CurrentEntry                            = i;
	    	ReferenceFrames[i].Field		= Field;
	    	ReferenceFrames[i].ParsedFrameParameters= ParsedFrameParameters;
	    	ReferenceFrames[i].DecodeFrameIndex     = ParsedFrameParameters->DecodeFrameIndex;
	    	ReferenceFrames[i].PicOrderCnt		= SliceHeader->PicOrderCnt;
	    	ReferenceFrames[i].PicOrderCntTop	= SliceHeader->PicOrderCntTop;
	    	ReferenceFrames[i].PicOrderCntBot	= SliceHeader->PicOrderCntBot;
	    	ReferenceFrames[i].ExtendedPicOrderCnt	= SliceHeader->ExtendedPicOrderCnt;
		ReferenceFrames[i].FrameNum		= FrameNum;

	        if( LongTerm )
	        {
		    ReferenceFrames[i].Usage            = UsedForLongTermReference;
		    ReferenceFrames[i].LongTermFrameIdx	= FrameNum;
		    NumLongTerm++;

		    if( Idr )
		        MaxLongTermFrameIdx             = 0;
	    	}
	    	else
	    	{
		    ReferenceFrames[i].Usage		= UsedForShortTermReference;
		    NumShortTerm++;

		    if( Idr )
		    	MaxLongTermFrameIdx		= NO_LONG_TERM_FRAME_INDICES;
	    	}

	    	break;
	    }
	}

    //
    // Now apply the appropriate list management algorithms
    //

    if( !Idr )
    {
	if( !ProcessControl )
	{
	    //
	    // Follow "Sliding window decoded reference picture marking process"
	    //

	    if( (NumLongTerm + NumShortTerm) > NumReferenceFrames )
	    {
		LowestFrameNumWrap      = 0x7fffffff;
		Entry                   = 0;

		for( i=0; i<(NumReferenceFrames+1); i++ )
		    if( (i != CurrentEntry) &&
			((ReferenceFrames[i].Usage & AnyUsedForShortTermReference) != 0) )
		    {
			FrameNumWrap    = (ReferenceFrames[i].FrameNum > FrameNum) ?
						ReferenceFrames[i].FrameNum - MaxFrameNum :
						ReferenceFrames[i].FrameNum;
			if( FrameNumWrap < LowestFrameNumWrap )
			{
			    LowestFrameNumWrap  = FrameNumWrap;
			    Entry               = i;
			}
		    }

		ReleaseReference( ActuallyReleaseReferenceFrames, Entry, AnyUsedForShortTermReference );

		if (NumShortTerm>100) report(severity_error,"%s: NumShortTerm references has gone negative\n",__FUNCTION__);
		if (NumLongTerm>100) report(severity_error,"%s: NumLongTerm references has gone negative\n",__FUNCTION__);
	    }
	}
	else
	{
	    //
	    // Follow "Adaptive memory control decoded reference picture marking process"
	    //
	    // First re-calculate picnum and long term pic num 
	    // NOTE forward play this may already be calculated, but in reverse it isn't.
	    //

	    MaxFrameNum         = 1 << (SliceHeader->SequenceParameterSet->log2_max_frame_num_minus4 + 4);

	    for( i=0; i<(NumReferenceFrames+1); i++ )
	    if( ReferenceFrames[i].Usage != NotUsedForReference )
            {
                FrameNumWrap				= (ReferenceFrames[i].FrameNum > SliceHeader->frame_num) ?
                                                        	ReferenceFrames[i].FrameNum - MaxFrameNum :
	                                                        ReferenceFrames[i].FrameNum;

		ReferenceFrames[i].PicNum		= Field ? (2*FrameNumWrap) : FrameNumWrap;
		ReferenceFrames[i].LongTermPicNum	= Field ? (2*ReferenceFrames[i].LongTermFrameIdx) : ReferenceFrames[i].LongTermFrameIdx;
	    }

#ifdef DUMP_REFLISTS
    report( severity_info, "\n    List of reference frames before MMCO (%d %d):-\n", NumShortTerm, NumLongTerm );
    for( i=0; i<(NumReferenceFrames+1); i++ )
	if( ReferenceFrames[i].Usage != NotUsedForReference )
	    report( severity_info, "\tDFI %5d, F/F %s, Usage %c%c, FrameNumber %4d (%4d), LongTermIndex %4d\n",
			ReferenceFrames[i].DecodeFrameIndex,
			(ReferenceFrames[i].Field ? "Field" : "Frame"),
			(((ReferenceFrames[i].Usage & UsedForTopLongTermReference) != 0) ? 'L' : (((ReferenceFrames[i].Usage & UsedForTopShortTermReference) != 0) ? 'S' : 'x')),
			(((ReferenceFrames[i].Usage & UsedForBotLongTermReference) != 0) ? 'L' : (((ReferenceFrames[i].Usage & UsedForBotShortTermReference) != 0) ? 'S' : 'x')),
			ReferenceFrames[i].FrameNum, ReferenceFrames[i].PicNum,
			((ReferenceFrames[i].Usage & AnyUsedForLongTermReference) != 0) ? ReferenceFrames[i].LongTermFrameIdx : 9999 );
#endif
	    for( i=0; ; i++ )
	    {
		if( MMC->memory_management_control_operation[i] == H264_MMC_END )
		    break;

#ifdef DUMP_REFLISTS
report( severity_info, "MMCO(%d %d %d) - %d - %3d %3d %3d %3d\n", Field, Top, FrameNum,
	MMC->memory_management_control_operation[i],
	MMC->difference_of_pic_nums_minus1[i],
	MMC->long_term_pic_num[i],
	MMC->long_term_frame_idx[i],
	MMC->max_long_term_frame_idx_plus1[i] );
#endif

#define FRAME_MMCO	0
#define FIELD_MMCO	128
		switch( MMC->memory_management_control_operation[i] + (Field ? FIELD_MMCO : FRAME_MMCO) )
		{
		    case H264_MMC_MARK_SHORT_TERM_UNUSED_FOR_REFERENCE + FRAME_MMCO:

			PicNumX = FrameNum - (MMC->difference_of_pic_nums_minus1[i] + 1);
			for( j=0; j<(NumReferenceFrames+1); j++ )
			    if( ((ReferenceFrames[j].Usage & AnyUsedForShortTermReference) != 0) &&
				(ReferenceFrames[j].PicNum == PicNumX) )
			    {
				ReleaseReference( ActuallyReleaseReferenceFrames, j, AnyUsedForShortTermReference );
				break;
			    }
			break;
//

		    case H264_MMC_MARK_SHORT_TERM_UNUSED_FOR_REFERENCE + FIELD_MMCO:

			PicNumX = 2 * FrameNum + 1 - (MMC->difference_of_pic_nums_minus1[i] + 1);
			for( j=0; j<(NumReferenceFrames+1); j++ )
			    if( ((ReferenceFrames[j].Usage & AnyUsedForShortTermReference) != 0) &&
				((ReferenceFrames[j].PicNum == PicNumX) || ((ReferenceFrames[j].PicNum + 1) == PicNumX)) )
			    {
				ReleaseCode	= (ReferenceFrames[j].PicNum == PicNumX) ? 
								OtherFieldUsedForShortTermReference :
								UsedForShortTermReference;
				ReleaseReference( ActuallyReleaseReferenceFrames, j, ReleaseCode );
				break;
			    }

			break;

//

		    case H264_MMC_MARK_LONG_TERM_UNUSED_FOR_REFERENCE  + FRAME_MMCO:

			for( j=0; j<(NumReferenceFrames+1); j++ )
			    if( ((ReferenceFrames[j].Usage & AnyUsedForLongTermReference) != 0) &&
				(ReferenceFrames[j].LongTermPicNum == MMC->long_term_pic_num[i]) )
			    {
				ReleaseReference( ActuallyReleaseReferenceFrames, j, AnyUsedForLongTermReference );
				break;
			    }

			break;

//

		    case H264_MMC_MARK_LONG_TERM_UNUSED_FOR_REFERENCE  + FIELD_MMCO:

			for( j=0; j<(NumReferenceFrames+1); j++ )
			    if( ((ReferenceFrames[j].Usage & AnyUsedForLongTermReference) != 0) &&
				((ReferenceFrames[j].LongTermPicNum == MMC->long_term_pic_num[i]) || ((ReferenceFrames[j].LongTermPicNum + 1) == MMC->long_term_pic_num[i])) )
			    {
				ReleaseCode	= (ReferenceFrames[j].LongTermPicNum == MMC->long_term_pic_num[i]) ? 
								OtherFieldUsedForLongTermReference :
								UsedForLongTermReference;
				ReleaseReference( ActuallyReleaseReferenceFrames, j, ReleaseCode );
				break;
			    }

			break;

//

		    case H264_MMC_ASSIGN_LONG_TERM_INDEX_TO_SHORT_TERM_PICTURE + FRAME_MMCO:

			// Release any frame already using the index
			for( j=0; j<(NumReferenceFrames+1); j++ )
			    if( ((ReferenceFrames[j].Usage & AnyUsedForLongTermReference) != 0) &&
				(ReferenceFrames[j].LongTermFrameIdx == MMC->long_term_frame_idx[i]) )
			    {
				ReleaseReference( ActuallyReleaseReferenceFrames, j, AnyUsedForLongTermReference );
				break;
			    }

			// Now assign the index
			PicNumX = FrameNum - (MMC->difference_of_pic_nums_minus1[i] + 1);
			for( j=0; j<(NumReferenceFrames+1); j++ )
			    if( ((ReferenceFrames[j].Usage & AnyUsedForShortTermReference) != 0) &&
				(ReferenceFrames[j].PicNum == PicNumX) )
			    {
				ReferenceFrames[j].LongTermFrameIdx      = MMC->long_term_frame_idx[i];
				ReferenceFrames[j].Usage                ^= UsedForShortTermReference;
				ReferenceFrames[j].Usage                |= UsedForLongTermReference;
				NumShortTerm--;
				NumLongTerm++;
				break;
			    }

			break;

//

		    case H264_MMC_ASSIGN_LONG_TERM_INDEX_TO_SHORT_TERM_PICTURE + FIELD_MMCO:

			// Here we release after the assignment, because we cannot release any complimentary field

			// Now assign the index
			PicNumX 		= 2 * FrameNum + 1 - (MMC->difference_of_pic_nums_minus1[i] + 1);
			LongTermIndexAssignedTo	= INVALID_INDEX;

			for( j=0; j<(NumReferenceFrames+1); j++ )
			    if( ((ReferenceFrames[j].Usage & AnyUsedForShortTermReference) != 0) &&
				((ReferenceFrames[j].PicNum == PicNumX) || ((ReferenceFrames[j].PicNum + 1) == PicNumX)) )
			    {
				LongTermIndexAssignedTo			 = j;
				ReferenceFrames[j].LongTermFrameIdx      = MMC->long_term_frame_idx[i];

				if( ReferenceFrames[j].PicNum == PicNumX )
				{
				    ReferenceFrames[j].Usage		^= OtherFieldUsedForShortTermReference;
				    ReferenceFrames[j].Usage		|= OtherFieldUsedForLongTermReference;
				}
				else
				{
				    ReferenceFrames[j].Usage		^= UsedForShortTermReference;
				    ReferenceFrames[j].Usage		|= UsedForLongTermReference;
				}

				if( (ReferenceFrames[j].Usage & AnyUsedForShortTermReference) == 0 )
				    NumShortTerm--;	// Now no short terms
				if( (ReferenceFrames[j].Usage & AnyUsedForLongTermReference) != AnyUsedForLongTermReference )
				    NumLongTerm++;	// Not gone to both fields long term
				break;
			    }

			// Release any frame already using the index
			for( j=0; j<(NumReferenceFrames+1); j++ )
			    if( ((ReferenceFrames[j].Usage & AnyUsedForLongTermReference) != 0) &&
				(ReferenceFrames[j].LongTermFrameIdx == MMC->long_term_frame_idx[i]) &&
				(j != LongTermIndexAssignedTo) )
			    {
				ReleaseReference( ActuallyReleaseReferenceFrames, j, AnyUsedForLongTermReference );
				break;
			    }

			break;

//

		    case H264_MMC_SPECIFY_MAX_LONG_TERM_INDEX + FRAME_MMCO:
		    case H264_MMC_SPECIFY_MAX_LONG_TERM_INDEX + FIELD_MMCO:

			// Release any frames greater than the max index
			for( j=0; j<(NumReferenceFrames+1); j++ )
			    if( ((ReferenceFrames[j].Usage & AnyUsedForLongTermReference) != 0) &&
				(ReferenceFrames[j].LongTermFrameIdx > (MMC->max_long_term_frame_idx_plus1[i] - 1)) )
				ReleaseReference( ActuallyReleaseReferenceFrames, j, AnyUsedForLongTermReference );

			MaxLongTermFrameIdx     = MMC->max_long_term_frame_idx_plus1[i] ?
						    (MMC->max_long_term_frame_idx_plus1[i] - 1) :
						    NO_LONG_TERM_FRAME_INDICES;
			break;

//

		    case H264_MMC_CLEAR + FRAME_MMCO:
		    case H264_MMC_CLEAR + FIELD_MMCO:

			for( j=0; j<(NumReferenceFrames+1); j++ )
			    if( j != CurrentEntry )
				ReleaseReference( ActuallyReleaseReferenceFrames, j, AnyUsedForReference );

			//
			// Post process current picture on a clear.
			//

			ReferenceFrames[CurrentEntry].PicOrderCnt	 = 0;
			ReferenceFrames[CurrentEntry].PicOrderCntTop	-= SliceHeader->PicOrderCnt;
			ReferenceFrames[CurrentEntry].PicOrderCntBot	-= SliceHeader->PicOrderCnt;

			ReferenceFrames[CurrentEntry].FrameNum		 = 0;
			AccumulatedFrameNumber				 = 0;
			PrevFrameNum					 = 0;

//

			NumLongTerm             = LongTerm ? 1 : 0;
			NumShortTerm            = LongTerm ? 0 : 1;
			MaxLongTermFrameIdx     = NO_LONG_TERM_FRAME_INDICES;

			break;

//

		    case H264_MMC_ASSIGN_LONG_TERM_INDEX_TO_CURRENT_DECODED_PICTURE + FRAME_MMCO:
		    case H264_MMC_ASSIGN_LONG_TERM_INDEX_TO_CURRENT_DECODED_PICTURE + FIELD_MMCO:

			// Release any frame already using the index
			for( j=0; j<(NumReferenceFrames+1); j++ )
			    if( ((ReferenceFrames[j].Usage & AnyUsedForLongTermReference) != 0)      &&
				(ReferenceFrames[j].LongTermFrameIdx == MMC->long_term_frame_idx[i]) &&
				(CurrentEntry != j) )
			    {
				ReleaseReference( ActuallyReleaseReferenceFrames, j, AnyUsedForLongTermReference );
				break;
			    }

			// Now assign the index
			ReferenceFrames[CurrentEntry].LongTermFrameIdx   = MMC->long_term_frame_idx[i];
			ReferenceFrames[CurrentEntry].Usage             &= ~UsedForShortTermReference;
			ReferenceFrames[CurrentEntry].Usage             |= UsedForLongTermReference;

			if( !LongTerm )
			{
			    if( (ReferenceFrames[CurrentEntry].Usage & AnyUsedForShortTermReference) == 0 )
				NumShortTerm--;		// Now no short terms

			    if( !Field || ((ReferenceFrames[CurrentEntry].Usage & AnyUsedForLongTermReference) != AnyUsedForLongTermReference) )
				NumLongTerm++;		// Not gone to both fields long term
			}
			break;
		}
	    }
	}

	if( (NumShortTerm + NumLongTerm) > NumReferenceFrames )
	{
#if 1
	    report( severity_error, "FrameParser_VideoH264_c::MarkReferencePictures - After MMCO operations, \n\t\tthere are more than the allowed number of reference frames.\n\t\tThe oldest will be discarded.\n" );
	    for( i=0,j=1; j<(NumReferenceFrames+1); j++ )
		if( ReferenceFrames[i].DecodeFrameIndex > ReferenceFrames[j].DecodeFrameIndex )
		    i = j;

	    ReleaseReference( true, i, AnyUsedForReference );
#else
	    report( severity_error, "FrameParser_VideoH264_c::MarkReferencePictures - After MMCO operations, \n\t\tthere are more than the allowed number of reference frames.\n\t\tAll but the current will be discarded.\n" );
	    for( j=0; j<(NumReferenceFrames+1); j++ )
		if( j != CurrentEntry )
		    ReleaseReference( ActuallyReleaseReferenceFrames, j, AnyUsedForReference );
#endif
	}

#ifdef DUMP_REFLISTS
    report( severity_info, "\n    List of reference frames After MMCO (%d %d):-\n", NumShortTerm, NumLongTerm );
    for( i=0; i<(NumReferenceFrames+1); i++ )
	if( ReferenceFrames[i].Usage != NotUsedForReference )
	    report( severity_info, "\tDFI %5d, F/F %s, Usage %c%c, FrameNumber %4d, LongTermIndex %4d\n",
			ReferenceFrames[i].DecodeFrameIndex,
			(ReferenceFrames[i].Field ? "Field" : "Frame"),
			(((ReferenceFrames[i].Usage & UsedForTopLongTermReference) != 0) ? 'L' : (((ReferenceFrames[i].Usage & UsedForTopShortTermReference) != 0) ? 'S' : 'x')),
			(((ReferenceFrames[i].Usage & UsedForBotLongTermReference) != 0) ? 'L' : (((ReferenceFrames[i].Usage & UsedForBotShortTermReference) != 0) ? 'S' : 'x')),
			ReferenceFrames[i].FrameNum,
			((ReferenceFrames[i].Usage & AnyUsedForLongTermReference) != 0) ? ReferenceFrames[i].LongTermFrameIdx : 9999 );
#endif
    }

//

#ifdef DUMP_REFLISTS
    report( severity_info, "\n    List of reference frames :-\n" );
    for( i=0; i<(NumReferenceFrames+1); i++ )
	if( ReferenceFrames[i].Usage != NotUsedForReference )
	    report( severity_info, "\tDFI %5d, F/F %s, Usage %c%c, FrameNumber %4d, LongTermIndex %4d\n",
			ReferenceFrames[i].DecodeFrameIndex,
			(ReferenceFrames[i].Field ? "Field" : "Frame"),
			(((ReferenceFrames[i].Usage & UsedForTopLongTermReference) != 0) ? 'L' : (((ReferenceFrames[i].Usage & UsedForTopShortTermReference) != 0) ? 'S' : 'x')),
			(((ReferenceFrames[i].Usage & UsedForBotLongTermReference) != 0) ? 'L' : (((ReferenceFrames[i].Usage & UsedForBotShortTermReference) != 0) ? 'S' : 'x')),
			ReferenceFrames[i].FrameNum,
			((ReferenceFrames[i].Usage & AnyUsedForLongTermReference) != 0) ? ReferenceFrames[i].LongTermFrameIdx : 9999 );
#endif

//

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The reset reference frame list function
//

FrameParserStatus_t   FrameParser_VideoH264_c::ResetReferenceFrameList( void )
{
    Player->CallInSequence( Stream, SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, CODEC_RELEASE_ALL );

    memset( &ReferenceFrames,      0x00, sizeof(H264ReferenceFrameData_t)  * (H264_MAX_REFERENCE_FRAMES + 1) );

    NumReferenceFrames          = 0;

    MaxLongTermFrameIdx         = 0;
    NumLongTerm                 = 0;
    NumShortTerm                = 0;

    ReferenceFrameList[0].EntryCount		= 0;
    ReferenceFrameList[1].EntryCount		= 0;
    ReferenceFrameListShortTerm[0].EntryCount	= 0;
    ReferenceFrameListShortTerm[1].EntryCount	= 0;
    ReferenceFrameListLongTerm.EntryCount	= 0;

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to purge deferred post decode parameter 
//      settings, these consist of the display frame index, and presentation 
//      time. Here we flush everything we have.
//

FrameParserStatus_t   FrameParser_VideoH264_c::ForPlayPurgeQueuedPostDecodeParameterSettings( void )
{
    ProcessDeferredDFIandPTSUpto( 0xffffffffffffffffull );
    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to process deferred post decode parameter 
//      settings, these consist of the display frame index, and presentation 
//      time, if we have an IDR or a B frame, we can process any frames with a 
//	lower pic order count, we also allow the processing of all frames upto 
//	a pic order cnt when a reference frame falls out of the sliding window
//	in reference framme marking.
//

FrameParserStatus_t   FrameParser_VideoH264_c::ForPlayProcessQueuedPostDecodeParameterSettings( void )
{
    //
    // Can we process any of these
    //

    if( ParsedFrameParameters->FirstParsedParametersForOutputFrame &&
	((SliceHeader->nal_unit_type == NALU_TYPE_IDR) ||
	!ParsedFrameParameters->ReferenceFrame) )
	ProcessDeferredDFIandPTSUpto( SliceHeader->ExtendedPicOrderCnt );

//

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to generate the post decode parameter 
//      settings, these consist of the display frame index, and presentation 
//      time, both of which may be deferred if the information is unavailable.
//
//      For h264, we allow the processing of all frames that are IDRs or 
//	non-reference frames, all others are deferred
//

FrameParserStatus_t   FrameParser_VideoH264_c::ForPlayGeneratePostDecodeParameterSettings( void )
{
    //
    // Default setting
    //

    InitializePostDecodeParameterSettings();

    //
    // Defer pts generation, and perform dts generation
    //

    DeferDFIandPTSGeneration( Buffer, ParsedFrameParameters, ParsedVideoParameters, SliceHeader->ExtendedPicOrderCnt );
    CalculateDts( ParsedFrameParameters, ParsedVideoParameters );

//

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific override function for doing a decode, we force 
//	the appropriate record to be generated in the extended picorder 
//	cnt table, then pass to the usual fn.
//
//	Note we only enter the deferred PTS tanble if the queue was sucessful
//

FrameParserStatus_t   FrameParser_VideoH264_c::RevPlayQueueFrameForDecode( void )
{
FrameParserStatus_t	Status;

//

    CalculateDts( ParsedFrameParameters, ParsedVideoParameters );

//

    Status	= FrameParser_Video_c::RevPlayQueueFrameForDecode();
    if( Status != FrameParserNoError )
	return Status;

//

    DeferDFIandPTSGeneration( Buffer, ParsedFrameParameters, ParsedVideoParameters, SliceHeader->ExtendedPicOrderCnt );

//

    return FrameParserNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific override function for processing decode stacks, performs the
//	standard play, then reinitializes variables appropriate for the next block.
//

FrameParserStatus_t   FrameParser_VideoH264_c::RevPlayProcessDecodeStacks( void )
{
FrameParserStatus_t	Status;

    Status	= FrameParser_Video_c::RevPlayProcessDecodeStacks();
    if( Status != FrameParserNoError )
	return Status;

    //
    // After a reverse jump, setup the data appropriately 
    //

    BehaveAsIfSeenAnIDR		= false;

    PrevPicOrderCntMsb          = 0;
    PrevPicOrderCntLsb          = 0;
    PrevFrameNum		= 0;
    PrevFrameNumOffset		= 0;

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//

FrameParserStatus_t   FrameParser_VideoH264_c::RevPlayGeneratePostDecodeParameterSettings( void )
{
H264SliceHeader_t		*SliceHeader;

//

    SliceHeader		= &((H264FrameParameters_t *)(ParsedFrameParameters->FrameParameterStructure))->SliceHeader;
    ProcessDeferredDFIandPTSDownto( SliceHeader->ExtendedPicOrderCnt );

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//

FrameParserStatus_t   FrameParser_VideoH264_c::RevPlayPurgeQueuedPostDecodeParameterSettings( void )
{
    ProcessDeferredDFIandPTSDownto( 0ull );
    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to manage a reference frame list in forward play
//      we only record a reference frame as such on the last field, in order to 
//      ensure the correct management of reference frames in the codec, we immediately 
//      inform the codec of a release on the first field of a field picture.
//

FrameParserStatus_t   FrameParser_VideoH264_c::ForPlayUpdateReferenceFrameList( void )
{

    //
    // Update the reference frame list only on the first slice of a reference frame
    //
 
    if( ParsedFrameParameters->ReferenceFrame )
    {
	SliceHeader		= &((H264FrameParameters_t *)(ParsedFrameParameters->FrameParameterStructure))->SliceHeader;
    	NumReferenceFrames	= SliceHeader->SequenceParameterSet->num_ref_frames;

	return MarkReferencePictures( true );
    }

//

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to add a frame to the reference 
//      frame list in reverse play. In H264 this is identical to 
//	the forward play mechanism.
//

FrameParserStatus_t   FrameParser_VideoH264_c::RevPlayAppendToReferenceFrameList( void )
{

    //
    // Update the reference frame list only on the first slice of a reference frame
    //
 
    if( ParsedFrameParameters->ReferenceFrame )
    {
	SliceHeader		= &((H264FrameParameters_t *)(ParsedFrameParameters->FrameParameterStructure))->SliceHeader;
    	NumReferenceFrames	= SliceHeader->SequenceParameterSet->num_ref_frames;

	return MarkReferencePictures( false );
    }

//

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

FrameParserStatus_t   FrameParser_VideoH264_c::RevPlayRemoveReferenceFrameFromList( void )
{
    Player->CallInSequence( Stream, SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, ParsedFrameParameters->DecodeFrameIndex );

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to junk the reference frame list
//

FrameParserStatus_t   FrameParser_VideoH264_c::RevPlayJunkReferenceFrameList( void )
{
    memset( &ReferenceFrames,      0x00, sizeof(H264ReferenceFrameData_t)  * (H264_MAX_REFERENCE_FRAMES + 1) );

    NumReferenceFrames          = 0;

    MaxLongTermFrameIdx         = 0;
    NumLongTerm                 = 0;
    NumShortTerm                = 0;

    ReferenceFrameList[0].EntryCount		= 0;
    ReferenceFrameList[1].EntryCount		= 0;
    ReferenceFrameListShortTerm[0].EntryCount	= 0;
    ReferenceFrameListShortTerm[1].EntryCount	= 0;
    ReferenceFrameListLongTerm.EntryCount	= 0;

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function for reverse play
//
//	I really struggle to explain what this does. Put simply when we jump
//	backwards a sequence at a time, any positive jump in the pic order 
//	counts that occurs during the processing of a sequence, needs 
//	to be reflected in those frames from the follow on sequence that 
//	were held due to unsatisfied reference frames (IE Open groups)
//

FrameParserStatus_t   FrameParser_VideoH264_c::RevPlayNextSequenceFrameProcess( void )
{
H264SliceHeader_t		*SliceHeader;
unsigned int			 Adjustment;

//

    SliceHeader		= &((H264FrameParameters_t *)(ParsedFrameParameters->FrameParameterStructure))->SliceHeader;

    switch( SliceHeader->SequenceParameterSet->pic_order_cnt_type )
    {
	case 0:
		Adjustment				 = LastExitPicOrderCntMsb - SliceHeader->EntryPicOrderCntMsb;
		if( Adjustment != 0 )
		{
		    ProcessDeferredDFIandPTSDownto( SliceHeader->ExtendedPicOrderCnt );

		    SliceHeader->PicOrderCntTop		+= Adjustment;
		    SliceHeader->PicOrderCntBot		+= Adjustment;
		    SliceHeader->PicOrderCnt		+= Adjustment;
		    SliceHeader->ExtendedPicOrderCnt	+= (long long)Adjustment;
		}

		LastExitPicOrderCntMsb			 = SliceHeader->ExitPicOrderCntMsb;
		if( !SliceHeader->ExitPicOrderCntMsbForced )
		    LastExitPicOrderCntMsb		+= Adjustment;

		break;

    }

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      On a slice code, we should have garnered all the data 
//      we require we for a frame decode, this function records that fact.
//

FrameParserStatus_t   FrameParser_VideoH264_c::CommitFrameForDecode( void )
{
FrameParserStatus_t			  Status;
H264SequenceParameterSetHeader_t	 *SPS;
bool					  FieldSequenceError;
PictureStructure_t      		  PictureStructure;
bool                    		  Frame;
unsigned int				  DeltaTfiDivisor;
unsigned int				  pic_struct;
bool					  ReferenceNatureChange;
bool					  AccumulatedNonPairedField;
SliceType_t				  SliceType;
unsigned int				  BaseDpbAdjustment;
unsigned char				  Policy;
MatrixCoefficientsType_t		  MatrixCoefficients;
Rational_t				  PixelAspectRatio;
bool					  ForceTopBottomInterlaced;

    //
    // Obtain useful values
    //

    SliceType			= SliceTypeTranslation[SliceHeader->slice_type];
    Frame			= SliceHeader->field_pic_flag == 0;
    PictureStructure		= Frame ? 
					StructureFrame :
					((SliceHeader->bottom_field_flag != 0) ? StructureBottomField : StructureTopField);

    //
    // Construct stream parameters
    //

    ParsedFrameParameters->NewStreamParameters                          = NewStreamParametersCheck();
    
    if( ParsedFrameParameters->NewStreamParameters )
    {
	Status	= PrepareNewStreamParameters();
	if( Status != FrameParserNoError )
	    return Status;
    }

    ParsedFrameParameters->SizeofStreamParameterStructure               = sizeof(H264StreamParameters_t);
    ParsedFrameParameters->StreamParameterStructure                     = StreamParameters;
    SPS									= StreamParameters->SequenceParameterSet;

    //
    // Check that the profile and level is supported
    //

    if( (SPS->profile_idc != H264_PROFILE_IDC_MAIN) && 
	(SPS->profile_idc != H264_PROFILE_IDC_HIGH) && 
	(SPS->profile_idc != H264_PROFILE_IDC_BASELINE) &&
	(SPS->constrained_set1_flag != 1) )
    {
	report( severity_error, "FrameParser_VideoH264_c::CommitFrameForDecode - Unsupported profile (profile_idc = %d, constrained_set1_flag = %d).\n", SPS->profile_idc, SPS->constrained_set1_flag );
	Player->MarkStreamUnPlayable( Stream );
	return FrameParserError;
    }

    //
    // Check that if the sps says we have a picture struct, then we actually have one
    //

    if( SPS->vui_seq_parameters.pict_struct_present_flag &&
	!SEIPictureTiming.Valid )
    {
	report( severity_error, "FrameParser_VideoH264_c::CommitFrameForDecode - SPS demands an SEI Picture timing Message, but none present.\n" );
    }

    //
    // Check that the aspect ratio code is valid for the specific standard
    //

    if( !inrange(SPS->vui_seq_parameters.aspect_ratio_idc, MIN_LEGAL_H264_ASPECT_RATIO_CODE, MAX_LEGAL_H264_ASPECT_RATIO_CODE) &&
	(SPS->vui_seq_parameters.aspect_ratio_idc != H264_ASPECT_RATIO_EXTENDED_SAR) )
    {
	report( severity_error, "FrameParser_VideoH264_c::CommitFrameForDecode - vui_seq_parameters has illegal value (%02x) for aspect_ratio_idc according to h264 standard.\n", SPS->vui_seq_parameters.aspect_ratio_idc );
	Player->MarkStreamUnPlayable( Stream );
	return FrameParserHeaderSyntaxError;
    }

//

    if( SPS->vui_seq_parameters.aspect_ratio_idc == H264_ASPECT_RATIO_UNSPECIFIED )
	PixelAspectRatio	= DefaultPixelAspectRatio;
    else if( SPS->vui_seq_parameters.aspect_ratio_idc == H264_ASPECT_RATIO_EXTENDED_SAR )
	PixelAspectRatio	= Rational_t( SPS->vui_seq_parameters.sar_width, SPS->vui_seq_parameters.sar_height );
    else
	PixelAspectRatio	= H264AspectRatios(SPS->vui_seq_parameters.aspect_ratio_idc);

    //
    // Perform first slice activities
    //

    FirstDecodeOfFrame                  = false;

    //
    // Calculate the crop units so any crop adjustment can be applied correctly
    // to frame widt/height and pan/scan offsets.
    //

    CalculateCropUnits();

    //
    // Calculate the pic order counts.
    // Must be done before copying the slice header, 
    // as these are recorded in there.
    //

    Status	= CalculatePicOrderCnts();
    if( Status != FrameParserNoError )
    {
	report( severity_error, "FrameParser_VideoH264_c::CommitFrameForDecode - Failed to calculate pic order counts.\n" );
	return Status;
    }

    //
    // Fill out the frame parameters structure
    //

    memcpy( &FrameParameters->SEIPanScanRectangle, &SEIPanScanRectangle, sizeof(H264SEIPanScanRectangle_t) );
    SEIPanScanRectangle.Valid	= 0;						// Use it once only

    ParsedFrameParameters->NewFrameParameters			= true;
    ParsedFrameParameters->SizeofFrameParameterStructure	= sizeof(H264FrameParameters_t);
    ParsedFrameParameters->FrameParameterStructure		= FrameParameters;

    //
    // Check to see if the previous field was non-paired
    //

    if( AccumulatedPictureStructure != StructureEmpty )
    {
	FieldSequenceError		= (AccumulatedPictureStructure == PictureStructure) ||
					  (PictureStructure == StructureFrame);

	ReferenceNatureChange		= AccumulatedReferenceField != (SliceHeader->nal_ref_idc != 0);

	AccumulatedNonPairedField	= FieldSequenceError ||
					  ReferenceNatureChange ||
					  (AccumulatedFrameNumber != SliceHeader->frame_num);

	if( AccumulatedNonPairedField )
	{
#if 0
report( severity_info, "NickQ NonPairedOutput - (%d %d) (%d %d) (%d %d)\n", 
	(AccumulatedPictureStructure == PictureStructure), (PictureStructure == StructureFrame),
	AccumulatedReferenceField, (SliceHeader->nal_ref_idc != 0),
	AccumulatedFrameNumber, SliceHeader->frame_num );
#endif

	    //
	    // Correct the non-paired field to be a whole frame.
	    //

	    if( DisplayOrderByDpbValues )
	    {
		BaseDpbValue++;
		SliceHeader->ExtendedPicOrderCnt++;
	    }

	    AccumulatedParsedVideoParameters->DisplayCount[0]		= 2;
	    AccumulatedParsedVideoParameters->PanScan.DisplayCount[0]	= 2;
	    NextDecodeFieldIndex++;

	    //
	    // Let the codec release it as a single field
	    //

	    Player->CallInSequence( Stream, SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnOutputPartialDecodeBuffers );
	    AccumulatedPictureStructure	= StructureEmpty;
	}
    }

    //
    // If we are doing field decode set appropriate flags, 
    // and accumulate data for detecting paired/non-paired fields.
    //

    FirstDecodeOfFrame			= (AccumulatedPictureStructure == StructureEmpty);
    AccumulatedPictureStructure		= (FirstDecodeOfFrame && !Frame) ? PictureStructure : StructureEmpty;
    AccumulatedFrameNumber		= SliceHeader->frame_num;
    AccumulatedReferenceField		= (SliceHeader->nal_ref_idc != 0);
    AccumulatedParsedVideoParameters	= ParsedVideoParameters;

    //
    // Deduce interlaced and top field first flags
    //

    if( ForceInterlacedProgressive )
    {
	DeducedInterlacedFlag		= ForcedInterlacedFlag;
	DeducedTopFieldFirst		= SliceHeader->PicOrderCntTop <= SliceHeader->PicOrderCntBot;
    }
    else if( FixDeducedFlags )
    {
	// Leave flags as set
    }
    else if( Frame )
    {
	DeducedInterlacedFlag		= SliceHeader->PicOrderCntTop != SliceHeader->PicOrderCntBot;
	DeducedTopFieldFirst		= SliceHeader->PicOrderCntTop <= SliceHeader->PicOrderCntBot;
    }
    else if( !FirstFieldSeen )
    {
	DeducedInterlacedFlag		= true;
	DeducedTopFieldFirst		= SliceHeader->bottom_field_flag == 0;
    }
    else if( !FirstDecodeOfFrame ) 
    {
	DeducedInterlacedFlag	= true;
	if( SliceHeader->bottom_field_flag != 0 )
	    DeducedTopFieldFirst	= LastFieldExtendedPicOrderCnt <= SliceHeader->ExtendedPicOrderCnt;
	else
	    DeducedTopFieldFirst	= SliceHeader->ExtendedPicOrderCnt <= LastFieldExtendedPicOrderCnt;
    }

    FirstFieldSeen			= true;
    LastFieldExtendedPicOrderCnt	= SliceHeader->ExtendedPicOrderCnt;

    //
    // Deduce the matrix coefficients for colour conversions
    //

    switch( SPS->vui_seq_parameters.matrix_coefficients )
    {
	case H264_MATRIX_COEFFICIENTS_BT709:		MatrixCoefficients	= MatrixCoefficients_ITU_R_BT709;	break;
	case H264_MATRIX_COEFFICIENTS_FCC:		MatrixCoefficients	= MatrixCoefficients_FCC;		break;
	case H264_MATRIX_COEFFICIENTS_BT470_BGI:	MatrixCoefficients	= MatrixCoefficients_ITU_R_BT470_2_BG;	break;
	case H264_MATRIX_COEFFICIENTS_SMPTE_170M:	MatrixCoefficients	= MatrixCoefficients_SMPTE_170M;	break;
	case H264_MATRIX_COEFFICIENTS_SMPTE_240M:	MatrixCoefficients	= MatrixCoefficients_SMPTE_240M;	break;

	case H264_MATRIX_COEFFICIENTS_IDENTITY:
	case H264_MATRIX_COEFFICIENTS_YCGCO:		report( severity_error, "FrameParser_VideoH264_c::CommitFrameForDecode - Unsupported matrix coefficient code specified (%02x).\n", SPS->vui_seq_parameters.matrix_coefficients );
							Player->MarkStreamUnPlayable( Stream );
							return FrameParserHeaderSyntaxError;

	default:
	case H264_MATRIX_COEFFICIENTS_RESERVED:
							report( severity_error, "FrameParser_VideoH264_c::CommitFrameForDecode - Reserved matrix coefficient code specified (%02x).\n", SPS->vui_seq_parameters.matrix_coefficients );
							// fall through

	case H264_MATRIX_COEFFICIENTS_UNSPECIFIED:	MatrixCoefficients	= MatrixCoefficients_Undefined;		break;
    }

    //
    // Record the stream and frame parameters into the appropriate structure
    //

    ParsedFrameParameters->FirstParsedParametersForOutputFrame          = FirstDecodeOfFrame;
    ParsedFrameParameters->FirstParsedParametersAfterInputJump          = FirstDecodeAfterInputJump;
    ParsedFrameParameters->SurplusDataInjected                          = SurplusDataInjected;
    ParsedFrameParameters->ContinuousReverseJump                        = ContinuousReverseJump;

    Policy								= Player->PolicyValue( Playback, Stream, PolicyH264AllowNonIDRResynchronization );
    ParsedFrameParameters->KeyFrame					= (Policy != PolicyValueApply) ?
										(SliceHeader->nal_unit_type == NALU_TYPE_IDR) :
										(SliceType == SliceTypeI);
    ParsedFrameParameters->IndependentFrame				= ParsedFrameParameters->KeyFrame || (SliceType == SliceTypeI);
    ParsedFrameParameters->ReferenceFrame                               = (SliceHeader->nal_ref_idc != 0);

//

    ParsedVideoParameters->SliceType					= SliceType;
    ParsedVideoParameters->FirstSlice					= true;
    ParsedVideoParameters->Content.Width                                = (SPS->pic_width_in_mbs_minus1 + 1) * 16;
    ParsedVideoParameters->Content.Height                               = (SPS->pic_height_in_map_units_minus1 + 1) * 16;
    if( !SPS->frame_mbs_only_flag )
	ParsedVideoParameters->Content.Height                           *= 2;

    //
    // Adjust width and height by crop values.  As this applies only to right and bottom_offset
    // we error if left_offset or top_offset are non-zero.
    //

    if( (SPS->frame_cropping_rect_top_offset != 0) || (SPS->frame_cropping_rect_left_offset != 0) )
    {
	report( severity_error, "FrameParser_VideoH264_c::CommitFrameForDecode - Uexpected frame cropping rectangle (%d, %d, %d, %d) - implementation error.\n",
			       SPS->frame_cropping_rect_left_offset, SPS->frame_cropping_rect_top_offset,
			       SPS->frame_cropping_rect_right_offset, SPS->frame_cropping_rect_bottom_offset );
    }
    ParsedVideoParameters->Content.Width                               -= (SliceHeader->CropUnitX * SPS->frame_cropping_rect_right_offset);
    ParsedVideoParameters->Content.Height                              -= (SliceHeader->CropUnitY * SPS->frame_cropping_rect_bottom_offset);

    ParsedVideoParameters->Content.DisplayWidth                         = ParsedVideoParameters->Content.Width;
    ParsedVideoParameters->Content.DisplayHeight                        = ParsedVideoParameters->Content.Height;

    ParsedVideoParameters->Content.OverscanAppropriate                  = SPS->vui_seq_parameters.overscan_appropriate_flag != 0;
    ParsedVideoParameters->Content.PixelAspectRatio                     = PixelAspectRatio;

    ParsedVideoParameters->Content.VideoFullRange			= SPS->vui_seq_parameters.video_full_range_flag;
    ParsedVideoParameters->Content.ColourMatrixCoefficients		= MatrixCoefficients;

    //
    // Extract information about nature of content and frame rate of display 
    //
    // --------------------------------------------------------------------------------------------------------------------
    //  Work out frame rate                    - see table E-6 in ISO/IEC 14496-10(E)
    // (E-6) Table
    // pict_struct_present_flag  field_pic_flag pic_struct DeltaTfiDivisor
    // 0                         1              -             1
    // 1                         -              1             1
    // 1                         -              2             1
    // 0                         0              -             2
    // 1                         -              0             2
    // 1                         -              3             2
    // 1                         -              4             2
    // 1                         -              5             3
    // 1                         -              6             3
    // 1                         -              7             4
    // 1                         -              8             6
    //

    ParsedVideoParameters->Content.FrameRate	= (SPS->vui_seq_parameters.num_units_in_tick == 0) ? 
							0 :
							Rational_t( SPS->vui_seq_parameters.time_scale, SPS->vui_seq_parameters.num_units_in_tick );

    if (SPS->vui_seq_parameters.fixed_frame_rate_flag )
    {
	pic_struct	= (SPS->vui_seq_parameters.pict_struct_present_flag && SEIPictureTiming.Valid) ?
				SEIPictureTiming.pic_struct :
				SEI_PICTURE_TIMING_PICSTRUCT_FRAME;

	switch( pic_struct )
	{
	// Frame rate is tied to progressive sequence or not, timing stuff is related to field counts
	    case SEI_PICTURE_TIMING_PICSTRUCT_TOP_FIELD:
	    case SEI_PICTURE_TIMING_PICSTRUCT_BOTTOM_FIELD:
	    case SEI_PICTURE_TIMING_PICSTRUCT_FRAME:
	    case SEI_PICTURE_TIMING_PICSTRUCT_TOP_BOTTOM:
	    case SEI_PICTURE_TIMING_PICSTRUCT_BOTTOM_TOP:
	    case SEI_PICTURE_TIMING_PICSTRUCT_TOP_BOTTOM_TOP:
	    case SEI_PICTURE_TIMING_PICSTRUCT_BOTTOM_TOP_BOTTOM:	DeltaTfiDivisor	= 2;		break;
	    case SEI_PICTURE_TIMING_PICSTRUCT_FRAME_DOUBLING:
	    case SEI_PICTURE_TIMING_PICSTRUCT_FRAME_TRIPLING:		DeltaTfiDivisor	= 1;		break;
	    default:							DeltaTfiDivisor	= 2;		break;
	}

	if( (ParsedVideoParameters->Content.FrameRate / DeltaTfiDivisor) > 20 )		// ********** JMW HACK  FOR DEMO (something about duff values) **********
	{
	    ParsedVideoParameters->Content.FrameRate	= ParsedVideoParameters->Content.FrameRate / DeltaTfiDivisor;
	}
    }

    //
    // This is a nick fixup, if no frame rate is encoded, or if the 
    // framerate is ridiculous, then we use the default value.
    //

    if ( UserSpecifiedDefaultFrameRate || !inrange( ParsedVideoParameters->Content.FrameRate, 8, 120 ) )
	ParsedVideoParameters->Content.FrameRate	= DefaultFrameRate;


    // --------------------------------------------------------------------------------------------------------------------
    //  Work out display flags                 - see table D-1 in ISO/IEC 14496-10(E)
    //  - A field picture is always displayed as interlace.
    //  - A frame picture is displayed according to the first condition encountered below
    //    - When SEI:pic_struct is present:
    //      - When SEI:ct_type is present:
    //        - 2 fields of a frame having SEI:ct_type = 0 or 2 are to displayed as progressive frame
    //        - 2 fields of a frame having SEI:ct_type = 1 are to be displayed as interlaced frame
    //      - When SEI:ct_type is not present:
    //        - SEI:pic_struct = 1,2 indicate interlace display
    //        - SEI:pic_struct = 0,5,6,7,8 indicate progressive display
    //        - SEI:pic_struct = (other values) indicate Progressive if TopFieldOrderCnt == BottomFieldOrderCntsame
    //    - When SEI:pic_struct is not present:
    //      - When for a coded frame, TopFieldOrderCnt = BottomFieldOrderCnt, this frame is displayed as a progressive frame
    //      - When for a coded frame, TopFieldOrderCnt != BottomFieldOrderCnt, this frame is displayed as a interlaced frame

    ParsedVideoParameters->PictureStructure		= PictureStructure;

    if( SPS->vui_seq_parameters.pict_struct_present_flag && SEIPictureTiming.Valid )
    {
	pic_struct	= SEIPictureTiming.pic_struct;
    }
    else
    {
	if( !Frame )
	    pic_struct	= (SliceHeader->bottom_field_flag == 0) ?
					SEI_PICTURE_TIMING_PICSTRUCT_TOP_FIELD :
					SEI_PICTURE_TIMING_PICSTRUCT_BOTTOM_FIELD;
	else if( SPS->frame_mbs_only_flag || (SliceHeader->PicOrderCntTop == SliceHeader->PicOrderCntBot) )
	    pic_struct	= SEI_PICTURE_TIMING_PICSTRUCT_FRAME;
	else
	    pic_struct	= (SliceHeader->PicOrderCntTop < SliceHeader->PicOrderCntBot) ?
					SEI_PICTURE_TIMING_PICSTRUCT_TOP_BOTTOM :
					SEI_PICTURE_TIMING_PICSTRUCT_BOTTOM_TOP;
    }

    //
    // Now generate the actual values from the specified or derived pic_struct
    //

    ForceTopBottomInterlaced	= Player->PolicyValue( Playback, Stream, PolicyH264TreatTopBottomPictureStructAsInterlaced ) == PolicyValueApply;

    switch( pic_struct )
    {
	default:
	case SEI_PICTURE_TIMING_PICSTRUCT_FRAME:
		    ParsedVideoParameters->Content.Progressive				= true;
		    ParsedVideoParameters->InterlacedFrame				= false;
		    ParsedVideoParameters->TopFieldFirst                                = true;
		    ParsedVideoParameters->DisplayCount[0]                              = 1;
		    ParsedVideoParameters->DisplayCount[1]                              = 0;
		    BaseDpbAdjustment							= 2;
		    break;

	case SEI_PICTURE_TIMING_PICSTRUCT_TOP_FIELD:
		    ParsedVideoParameters->Content.Progressive				= false;
		    ParsedVideoParameters->InterlacedFrame				= ForceTopBottomInterlaced ? true : DeducedInterlacedFlag;
		    ParsedVideoParameters->TopFieldFirst                                = DeducedTopFieldFirst;
		    ParsedVideoParameters->DisplayCount[0]                              = 1;
		    ParsedVideoParameters->DisplayCount[1]                              = 0;
		    BaseDpbAdjustment							= 1;
		    break;

	case SEI_PICTURE_TIMING_PICSTRUCT_BOTTOM_FIELD:
		    ParsedVideoParameters->Content.Progressive				= false;
		    ParsedVideoParameters->InterlacedFrame				= ForceTopBottomInterlaced ? true : DeducedInterlacedFlag;
		    ParsedVideoParameters->TopFieldFirst                                = DeducedTopFieldFirst;
		    ParsedVideoParameters->DisplayCount[0]                              = 1;
		    ParsedVideoParameters->DisplayCount[1]                              = 0;
		    BaseDpbAdjustment							= 1;
		    break;

	case SEI_PICTURE_TIMING_PICSTRUCT_TOP_BOTTOM:
		    ParsedVideoParameters->Content.Progressive				= false;
		    ParsedVideoParameters->InterlacedFrame				= ForceTopBottomInterlaced ? true : DeducedInterlacedFlag;
		    ParsedVideoParameters->TopFieldFirst                                = true;
		    ParsedVideoParameters->DisplayCount[0]                              = 1;
		    ParsedVideoParameters->DisplayCount[1]                              = 1;
		    BaseDpbAdjustment							= 2;
		    break;

	case SEI_PICTURE_TIMING_PICSTRUCT_BOTTOM_TOP:
		    ParsedVideoParameters->Content.Progressive				= false;
		    ParsedVideoParameters->InterlacedFrame				= ForceTopBottomInterlaced ? true : DeducedInterlacedFlag;
		    ParsedVideoParameters->TopFieldFirst                                = false;
		    ParsedVideoParameters->DisplayCount[0]                              = 1;
		    ParsedVideoParameters->DisplayCount[1]                              = 1;
		    BaseDpbAdjustment							= 2;
		    break;

	case SEI_PICTURE_TIMING_PICSTRUCT_TOP_BOTTOM_TOP:
		    ParsedVideoParameters->Content.Progressive				= false;
		    ParsedVideoParameters->InterlacedFrame				= false;
		    ParsedVideoParameters->TopFieldFirst                                = true;
		    ParsedVideoParameters->DisplayCount[0]                              = 2;
		    ParsedVideoParameters->DisplayCount[1]                              = 1;
		    BaseDpbAdjustment							= 3;
		    break;

	case SEI_PICTURE_TIMING_PICSTRUCT_BOTTOM_TOP_BOTTOM:
		    ParsedVideoParameters->Content.Progressive				= false;
		    ParsedVideoParameters->InterlacedFrame				= false;
		    ParsedVideoParameters->TopFieldFirst                                = false;
		    ParsedVideoParameters->DisplayCount[0]                              = 2;
		    ParsedVideoParameters->DisplayCount[1]                              = 1;
		    BaseDpbAdjustment							= 3;
		    break;

	case SEI_PICTURE_TIMING_PICSTRUCT_FRAME_DOUBLING:
		    ParsedVideoParameters->Content.Progressive				= true;
		    ParsedVideoParameters->InterlacedFrame				= false;
		    ParsedVideoParameters->TopFieldFirst                                = true;
		    ParsedVideoParameters->DisplayCount[0]                              = 2;
		    ParsedVideoParameters->DisplayCount[1]                              = 0;
		    BaseDpbAdjustment							= 4;
		    break;

	case SEI_PICTURE_TIMING_PICSTRUCT_FRAME_TRIPLING:
		    ParsedVideoParameters->Content.Progressive				= true;
		    ParsedVideoParameters->InterlacedFrame				= false;
		    ParsedVideoParameters->TopFieldFirst                                = true;
		    ParsedVideoParameters->DisplayCount[0]                              = 3;
		    ParsedVideoParameters->DisplayCount[1]                              = 0;
		    BaseDpbAdjustment							= 6;
		    break;
    }

    //
    // Check deductions - noting that pict struct, if present,
    // just overrides the deduction completely.
    //

    if( SPS->vui_seq_parameters.pict_struct_present_flag && SEIPictureTiming.Valid )
    {
	DeducedInterlacedFlag	= ParsedVideoParameters->InterlacedFrame;
	DeducedTopFieldFirst	= ParsedVideoParameters->TopFieldFirst;
	FixDeducedFlags		= true;
    }

    if( (ParsedVideoParameters->InterlacedFrame != DeducedInterlacedFlag) ||
	(ParsedVideoParameters->TopFieldFirst   != DeducedTopFieldFirst) )
	report( severity_info, "ZZZ FrameParser_VideoH264_c::CommitFrameForDecode - Deduction do not match reality (%d %d -> %d %d), %d %2d\n",
		DeducedInterlacedFlag, DeducedTopFieldFirst, ParsedVideoParameters->InterlacedFrame, ParsedVideoParameters->TopFieldFirst,
		SPS->vui_seq_parameters.pict_struct_present_flag, pic_struct );

    //
    // If this is the first slice, then adjust the BaseDpbValue
    //

    BaseDpbValue	+= BaseDpbAdjustment;

    //
    // Record our claim on both the frame and stream parameters
    //

    Buffer->AttachBuffer( StreamParametersBuffer );
    Buffer->AttachBuffer( FrameParametersBuffer );

    //
    // Check wether or not we can safely handle reverse play,
    //		Cannot reverse with B reference frames
    //		Cannot currently reverse field pictures
    //

    if( Configuration.SupportSmoothReversePlay &&
	((ParsedFrameParameters->ReferenceFrame && (SliceType == SliceTypeB)) ||
	 !Frame) )
    {
	Configuration.SupportSmoothReversePlay	= false;
	report( severity_info, "FrameParser_VideoH264_c::CommitFrameForDecode - Detected inability to handle smooth reverse.\n" );
    }

    //
    // Finally set the appropriate flag and return
    //

    FrameToDecode       	= true;

#if 0
{
    report( severity_info, "NickQ %3d   - PStruct = %d (%d %d %d - %d %d) - Ref %d %d Fn %2d Dpb (%2d + %2d) = %2d Extended Dpb %16llx\n",
	NextDecodeFrameIndex,
        ParsedVideoParameters->PictureStructure,
        ParsedVideoParameters->Content.Progressive,
        ParsedVideoParameters->InterlacedFrame, ParsedVideoParameters->TopFieldFirst,
        ParsedVideoParameters->DisplayCount[0], ParsedVideoParameters->DisplayCount[1],
	ParsedFrameParameters->KeyFrame, ParsedFrameParameters->ReferenceFrame,
	SliceHeader->frame_num,
	BaseDpbValue, SEIPictureTiming.dpb_output_delay, BaseDpbValue + SEIPictureTiming.dpb_output_delay, SliceHeader->ExtendedPicOrderCnt );
}
#endif

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Boolean function to evaluate whether or not the stream 
//      parameters are new.
//

bool   FrameParser_VideoH264_c::NewStreamParametersCheck( void )
{
bool            Different;

    //
    // If we haven't got any previous then they must be new.
    //

    if( StreamParameters == NULL )
	return true;

    //
    // If we haven't overwritten any, and the pointers are unchanged, then 
    // they must the same, otherwise do a mem compare to check for differences.
    //

    Different		= false;

    if( ReadNewSPS || (StreamParameters->SequenceParameterSet != SliceHeader->SequenceParameterSet) )
	Different	= memcmp( &CopyOfSequenceParameterSet, SliceHeader->SequenceParameterSet, sizeof(H264SequenceParameterSetHeader_t) ) != 0;

    if( !Different && (ReadNewSPSExtension || (StreamParameters->SequenceParameterSetExtension != SliceHeader->SequenceParameterSetExtension)) )
	Different	= memcmp( &CopyOfSequenceParameterSetExtension, SliceHeader->SequenceParameterSetExtension, sizeof(H264SequenceParameterSetExtensionHeader_t) ) != 0;

    if( !Different && (ReadNewPPS || (StreamParameters->PictureParameterSet != SliceHeader->PictureParameterSet)) )
	Different	= memcmp( &CopyOfPictureParameterSet, SliceHeader->PictureParameterSet, sizeof(H264PictureParameterSetHeader_t) ) != 0;

    //
    // Do we need to copy over new versions
    //

    ReadNewSPS			= false;
    ReadNewSPSExtension		= false;
    ReadNewPPS			= false;

    if( Different )
    {
	memcpy( &CopyOfSequenceParameterSet, SliceHeader->SequenceParameterSet, sizeof(H264SequenceParameterSetHeader_t) );
	memcpy( &CopyOfSequenceParameterSetExtension, SliceHeader->SequenceParameterSetExtension, sizeof(H264SequenceParameterSetExtensionHeader_t) );
	memcpy( &CopyOfPictureParameterSet, SliceHeader->PictureParameterSet, sizeof(H264PictureParameterSetHeader_t) );
	return true;
    }
    else
    {
	//
	// Reduces the number of times we do the check when streams 
        // resend the same picture parameter set, without changing it (transformers 7028).
	//

	StreamParameters->SequenceParameterSet 		= SliceHeader->SequenceParameterSet;
	StreamParameters->SequenceParameterSetExtension	= SliceHeader->SequenceParameterSetExtension;
	StreamParameters->PictureParameterSet		= SliceHeader->PictureParameterSet;
    }

//

    return false;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Boolean function to evaluate whether or not the stream 
//      parameters are new.
//

FrameParserStatus_t   FrameParser_VideoH264_c::PrepareNewStreamParameters( void )
{
FrameParserStatus_t	Status;

//

    Status	= GetNewStreamParameters( (void **)(&StreamParameters) );
    if( Status != FrameParserNoError )
	return Status;

//

    StreamParameters->SequenceParameterSet		= SliceHeader->SequenceParameterSet;
    StreamParameters->SequenceParameterSetExtension 	= SliceHeader->SequenceParameterSetExtension;
    StreamParameters->PictureParameterSet		= SliceHeader->PictureParameterSet;

    //
    // Attch the picture parameter set, note the sequence 
    // parameter set, and its extension, is already attached 
    // to the picture parameter set.
    //

    StreamParametersBuffer->AttachBuffer( PictureParameterSetTable[SliceHeader->pic_parameter_set_id].Buffer );

//

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to insert an entry into the deferred DFI/PTS list
//	in an ordered position
//

void    FrameParser_VideoH264_c::DeferDFIandPTSGeneration(
							Buffer_t		 Buffer,
							ParsedFrameParameters_t	*ParsedFrameParameters,
							ParsedVideoParameters_t	*ParsedVideoParameters,
							unsigned long long	 ExtendedPicOrderCnt )
{
unsigned int	i;
unsigned int 	Index;
unsigned char   SpecialCaseDpb;
unsigned int	MaxDeferals;
unsigned int    HeldReferenceFrames;
unsigned int	InvalidPTSSequence;

    //
    // Adjust pic order cnt for fields given same pic order cnt
    // Always for PicOrderCnt, only when specified for Dpb
    //

    ExtendedPicOrderCnt		= (ExtendedPicOrderCnt << 1);

    if( !DisplayOrderByDpbValues )
    {
	if( (ParsedVideoParameters->PictureStructure != StructureFrame) &&
	    ((ParsedVideoParameters->PictureStructure == StructureTopField) != ParsedVideoParameters->TopFieldFirst) )
	    ExtendedPicOrderCnt++;
    }
    else if( ParsedFrameParameters->ReferenceFrame )
    {
	SpecialCaseDpb		= Player->PolicyValue( Playback, Stream, PolicyH264TreatDuplicateDpbValuesAsNonReferenceFrameFirst );
	if( SpecialCaseDpb == PolicyValueApply )
	    ExtendedPicOrderCnt++;
    }

    //
    // Fill in our list entry
    //

    Buffer->IncrementReferenceCount();
    Buffer->GetIndex( &Index );

    if( DeferredList[Index].Buffer != NULL )
	report( severity_error, "FrameParser_VideoH264_c::DeferDFIandPTSGeneration - DeferredList entry already used - Implementation error.\n" );

    DeferredList[Index].Buffer			= Buffer;
    DeferredList[Index].ParsedFrameParameters	= ParsedFrameParameters;
    DeferredList[Index].ParsedVideoParameters	= ParsedVideoParameters;
    DeferredList[Index].ExtendedPicOrderCnt	= ExtendedPicOrderCnt;

    //
    // Where do we sit
    //

    for( i=0; i<DeferredListEntries; i++ )
	if( ExtendedPicOrderCnt < DeferredList[OrderedDeferredList[i]].ExtendedPicOrderCnt )
	    break;

    if( i < DeferredListEntries )
	memmove( &OrderedDeferredList[i+1], &OrderedDeferredList[i], (DeferredListEntries - i) * sizeof(unsigned int) );

    OrderedDeferredList[i]	= Index;
    DeferredListEntries++;

    //
    // Perform PTS validation - check for a jump in PTS of the wrong direction
    //				then range check it to less than 1 second to 
    //				allow a major jump (IE loop, add insert etc)
    //

#if 1
    if( ValidTime(ParsedFrameParameters->NormalizedPlaybackTime) )
    {
	unsigned long long	PTS = ParsedFrameParameters->NormalizedPlaybackTime;
	bool			Jump;

	InvalidPTSSequence	= false;

	// Check those before me in the list
	Jump	= (i != 0) && 
		  ValidTime(DeferredList[OrderedDeferredList[i-1]].ParsedFrameParameters->NormalizedPlaybackTime) &&
		  (PTS < DeferredList[OrderedDeferredList[i-1]].ParsedFrameParameters->NormalizedPlaybackTime);

	if( Jump && ((DeferredList[OrderedDeferredList[i-1]].ParsedFrameParameters->NormalizedPlaybackTime - PTS) < 1000000) )
	    InvalidPTSSequence	= true;
    
	// Check those after me in the list
	Jump	= (i < (DeferredListEntries-1)) && 
		  ValidTime(DeferredList[OrderedDeferredList[i+1]].ParsedFrameParameters->NormalizedPlaybackTime) &&
		  (PTS > DeferredList[OrderedDeferredList[i+1]].ParsedFrameParameters->NormalizedPlaybackTime);

	if( Jump && ((PTS - DeferredList[OrderedDeferredList[i+1]].ParsedFrameParameters->NormalizedPlaybackTime) < 1000000) )
	    InvalidPTSSequence	= true;

	if( InvalidPTSSequence && !DpbValuesInvalidatedByPTS )
	{
	    report( severity_error, "H264 Dpb values incompatible with PTS ordering, falling back to frame re-ordering based on PicOrderCnt values.\n" );
	    DpbValuesInvalidatedByPTS		 = true;
	    DisplayOrderByDpbValues		 = false;
	    PicOrderCntOffset			+= PicOrderCntOffsetAdjust;
	}
    }
#endif

    //
    // Limit the deferal process with respect to the number of available decode buffers
    //
    //	    Start with the basic buffer count,
    //	    Subtract 2 for buffers committed to manifestor (display and de-interlace)
    //	    Then for each reference frame already allocated a display index, but held
    //	    as a reference subtract one more.
    //

    if( PlaybackDirection == PlayForward )
	Manifestor->GetDecodeBufferCount( &MaxDeferals );
    else
	MaxDeferals	= H264_CODED_FRAME_COUNT;

#if 0
//
// I really think I should deploy this, but I have now reduced 
// the streams that might break without it so I am leaving it out.
//
#define FRAMES_HELD_BY_MANIFESTOR	2
    HeldReferenceFrames	 = 0;

    for( i=0; i<(NumReferenceFrames+1); i++ )
	if( ReferenceFrames[i].Usage != NotUsedForReference )
	    if( (ReferenceFrames[i].ParsedFrameParameters->DisplayFrameIndex != INVALID_INDEX) &&
		(ReferenceFrames[i].ParsedFrameParameters->DisplayFrameIndex < (NextDisplayFrameIndex-FRAMES_HELD_BY_MANIFESTOR)) )
		HeldReferenceFrames++;
#else
#define FRAMES_HELD_BY_MANIFESTOR	0
    HeldReferenceFrames	 = 0;
#endif

//report( severity_info, "Defer - %4d, Ref = %4d, Max = %4d\n", DeferredListEntries, HeldReferenceFrames, MaxDeferals ); 

    if( (DeferredListEntries + FRAMES_HELD_BY_MANIFESTOR + HeldReferenceFrames) >= MaxDeferals )
    {
	report( severity_error, "FrameParser_VideoH264_c::DeferDFIandPTSGeneration - Unable to defer, too many outstanding. \n\t\tThere may be too few decode buffers to handle this stream (%d + %d + %d >= %d).\n", 
		DeferredListEntries, FRAMES_HELD_BY_MANIFESTOR, HeldReferenceFrames, MaxDeferals );

	if( PlaybackDirection == PlayForward )
	    ProcessDeferredDFIandPTSUpto( (DeferredList[OrderedDeferredList[0]].ExtendedPicOrderCnt>>1) + 1 );
	else
	    ProcessDeferredDFIandPTSDownto( (DeferredList[OrderedDeferredList[DeferredListEntries-1]].ExtendedPicOrderCnt>>1) + 1 ); 
    }
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to process entries in the defereed DFI/PTS list
//

void   FrameParser_VideoH264_c::ProcessDeferredDFIandPTSUpto( unsigned long long 	 ExtendedPicOrderCnt )
{
unsigned int		i, j;
unsigned int		Index;
unsigned int		LeastPlaybackTimeIndex;
unsigned long long	LeastPlaybackTime;
bool			UsePTSFrameRate;

    //
    // Adjust pic order cnt for fields given same pic order cnt
    //

    ExtendedPicOrderCnt		= (ExtendedPicOrderCnt << 1);

    //
    // Process them
    //

#if 1
//--------------------------------------------------------------

    for( i=0;
	 (i<DeferredListEntries) && (DeferredList[OrderedDeferredList[i]].ExtendedPicOrderCnt < ExtendedPicOrderCnt);
	 )
    {
	if( OrderedDeferredList[i] == INVALID_INDEX )
	{
	    i++;
	    continue;
	}

	//
	// Check through the PTS values, is anyone earlier
	//

	LeastPlaybackTimeIndex	= i;
	LeastPlaybackTime	= DeferredList[OrderedDeferredList[i]].ParsedFrameParameters->NormalizedPlaybackTime;

	if( LeastPlaybackTime != INVALID_TIME )
	{
	    for( j=i+1; j<DeferredListEntries; j++ )
		if( (OrderedDeferredList[j] != INVALID_INDEX) &&
		    ValidTime(DeferredList[OrderedDeferredList[j]].ParsedFrameParameters->NormalizedPlaybackTime) && 
		    (DeferredList[OrderedDeferredList[j]].ParsedFrameParameters->NormalizedPlaybackTime < LeastPlaybackTime) )
		{
		    LeastPlaybackTimeIndex	= j;
		    LeastPlaybackTime		= DeferredList[OrderedDeferredList[j]].ParsedFrameParameters->NormalizedPlaybackTime;
		}
	}

        //
	// Process the appropriate one
	//

	Index		= OrderedDeferredList[LeastPlaybackTimeIndex];
	CalculateFrameIndexAndPts( DeferredList[Index].ParsedFrameParameters, DeferredList[Index].ParsedVideoParameters );
	SetupPanScanValues( DeferredList[Index].ParsedFrameParameters, DeferredList[Index].ParsedVideoParameters );
	DeferredList[Index].Buffer->DecrementReferenceCount();
	DeferredList[Index].Buffer	= NULL;

	//
	// Now move on if we did not find an earlier one, otherwise clear that entry in the list
	//

	if( LeastPlaybackTimeIndex == i )
	    i++;
	else
	    OrderedDeferredList[LeastPlaybackTimeIndex]	= INVALID_INDEX;
    }

    //
    // Shuffle up the ordered list
    //

    if( i != 0 )
    {
	for( j=0; i<DeferredListEntries; i++ )
	    if( OrderedDeferredList[i] != INVALID_INDEX )
		OrderedDeferredList[j++]	=  OrderedDeferredList[i];

	DeferredListEntries	= j;
    }


//--------------------------------------------------------------

#else
    for( i=0;
	 (i<DeferredListEntries) && (DeferredList[OrderedDeferredList[i]].ExtendedPicOrderCnt < ExtendedPicOrderCnt);
	 i++ )
    {
	Index	= OrderedDeferredList[i];
	CalculateFrameIndexAndPts( DeferredList[Index].ParsedFrameParameters, DeferredList[Index].ParsedVideoParameters );
	SetupPanScanValues( DeferredList[Index].ParsedFrameParameters, DeferredList[Index].ParsedVideoParameters );
	DeferredList[Index].Buffer->DecrementReferenceCount();
	DeferredList[Index].Buffer	= NULL;
    }

    //
    // Shuffle up the ordered list
    //

    if( i != 0 )
    {
	memmove( &OrderedDeferredList[0], &OrderedDeferredList[i], (DeferredListEntries - i) * sizeof(unsigned int) );
	DeferredListEntries	-= i;
    }
#endif

    //
    // Do we need to update the default frame rate based on PTS values
    //

    UsePTSFrameRate		= !UserSpecifiedDefaultFrameRate &&
				  ValidPTSDeducedFrameRate &&
				  (Player->PolicyValue( Playback, Stream, PolicyUsePTSDeducedDefaultFrameRates ) == PolicyValueApply);

    if( UsePTSFrameRate )
	DefaultFrameRate	= PTSDeducedFrameRate;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to process entries in the defereed DFI/PTS list
//

void   FrameParser_VideoH264_c::ProcessDeferredDFIandPTSDownto( unsigned long long 	 ExtendedPicOrderCnt )
{
unsigned int		i, j;
unsigned int		Index;
unsigned int		GreatestPlaybackTimeIndex;
unsigned long long	GreatestPlaybackTime;
bool			UsePTSFrameRate;

    //
    // Adjust pic order cnt for fields given same pic order cnt
    //

    ExtendedPicOrderCnt		= (ExtendedPicOrderCnt << 1) | 1;

    //
    // Process them
    //

#if 1
//--------------------------------------------------------------

    for( ;
	 (DeferredListEntries>0) && (DeferredList[OrderedDeferredList[DeferredListEntries-1]].ExtendedPicOrderCnt > ExtendedPicOrderCnt);
	 )
    {
	if( OrderedDeferredList[DeferredListEntries-1] == INVALID_INDEX )
	{
	    DeferredListEntries--;
	    continue;
	}

	//
	// Check through the PTS values, is anyone later
	//

	GreatestPlaybackTimeIndex	= DeferredListEntries-1;
	GreatestPlaybackTime		= DeferredList[OrderedDeferredList[DeferredListEntries-1]].ParsedFrameParameters->NormalizedPlaybackTime;

	if( GreatestPlaybackTime != INVALID_TIME )
	{
	    for( i=0; i<(DeferredListEntries-1); i++ )
		if( (OrderedDeferredList[i] != INVALID_INDEX) &&
		    ValidTime(DeferredList[OrderedDeferredList[i]].ParsedFrameParameters->NormalizedPlaybackTime) && 
		    (DeferredList[OrderedDeferredList[i]].ParsedFrameParameters->NormalizedPlaybackTime > GreatestPlaybackTime) )
		{
		    GreatestPlaybackTimeIndex	= i;
		    GreatestPlaybackTime	= DeferredList[OrderedDeferredList[i]].ParsedFrameParameters->NormalizedPlaybackTime;
		}
	}

        //
	// Process the appropriate one
	//

	Index		= OrderedDeferredList[GreatestPlaybackTimeIndex];
	CalculateFrameIndexAndPts( DeferredList[Index].ParsedFrameParameters, DeferredList[Index].ParsedVideoParameters );
	SetupPanScanValues( DeferredList[Index].ParsedFrameParameters, DeferredList[Index].ParsedVideoParameters );
	DeferredList[Index].Buffer->DecrementReferenceCount();
	DeferredList[Index].Buffer	= NULL;

	//
	// Now move on if we did not find an earlier one, otherwise clear that entry in the list
	//

	if( GreatestPlaybackTimeIndex == (DeferredListEntries-1) )
	    DeferredListEntries--;
	else
	    OrderedDeferredList[GreatestPlaybackTimeIndex]	= INVALID_INDEX;
    }

    //
    // Shuffle down the ordered list (just closing any invalid indices up)
    //

    if( DeferredListEntries != 0 )
    {
	for( i=j=0; i<DeferredListEntries; i++ )
	    if( OrderedDeferredList[i] != INVALID_INDEX )
		OrderedDeferredList[j++]	=  OrderedDeferredList[i];

	DeferredListEntries	= j;
    }

//--------------------------------------------------------------

#else
    while( (DeferredListEntries>0) && (DeferredList[OrderedDeferredList[DeferredListEntries-1]].ExtendedPicOrderCnt > ExtendedPicOrderCnt) )
    {
	DeferredListEntries--;
	Index	= OrderedDeferredList[DeferredListEntries];
	CalculateFrameIndexAndPts( DeferredList[Index].ParsedFrameParameters, DeferredList[Index].ParsedVideoParameters );
	SetupPanScanValues( DeferredList[Index].ParsedFrameParameters, DeferredList[Index].ParsedVideoParameters );
	DeferredList[Index].Buffer->DecrementReferenceCount();
	DeferredList[Index].Buffer	= NULL;
    }
#endif

    //
    // Do we need to update the default frame rate based on PTS values
    //

    UsePTSFrameRate		= !UserSpecifiedDefaultFrameRate &&
				  ValidPTSDeducedFrameRate &&
				  (Player->PolicyValue( Playback, Stream, PolicyUsePTSDeducedDefaultFrameRates ) == PolicyValueApply);

    if( UsePTSFrameRate )
	DefaultFrameRate	= PTSDeducedFrameRate;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to process the pan scan parameters in output order
//

void   FrameParser_VideoH264_c::SetupPanScanValues(		ParsedFrameParameters_t	 *ParsedFrameParameters,
								ParsedVideoParameters_t	 *ParsedVideoParameters )
{
unsigned int                     i;
bool                             PanScanIsOn;
H264SliceHeader_t               *SliceHeader;
H264SEIPanScanRectangle_t       *SEIPanScanRectangle;
unsigned char                    DisplayFormat;
unsigned char                    DisplayAspectRatio;
unsigned int                     FrameHeightInMbs;
unsigned int                     Left;
unsigned int                     Right;
unsigned int                     Top;
unsigned int                     Bottom;

//

    SliceHeader		= &((H264FrameParameters_t *)(ParsedFrameParameters->FrameParameterStructure))->SliceHeader;
    SEIPanScanRectangle	= &((H264FrameParameters_t *)(ParsedFrameParameters->FrameParameterStructure))->SEIPanScanRectangle;

    //
    // Process the message
    //

    if( SEIPanScanRectangle->Valid &&
	(inrange(SEIPanScanRectangle->pan_scan_rect_id,0,255) || inrange(SEIPanScanRectangle->pan_scan_rect_id,512,0x7fffffff)) )
    {
	//
	// Is this just a cancel flag
	//

	if( SEIPanScanRectangle->pan_scan_rect_cancel_flag )
	{
	    PanScanState.Count			= 0;
	}
	else
	{
	    PanScanState.Count                  = SEIPanScanRectangle->pan_scan_cnt_minus1 + 1;
	    PanScanState.RepetitionPeriod       = SEIPanScanRectangle->pan_scan_rect_repetition_period;

	    FrameHeightInMbs	= ((2 - SliceHeader->SequenceParameterSet->frame_mbs_only_flag) * 
				  (SliceHeader->SequenceParameterSet->pic_height_in_map_units_minus1 + 1)) /
				  (1 + SliceHeader->field_pic_flag);

	    for( i=0; i<PanScanState.Count; i++ )
	    {
#if 1
		Left                            = 16 * SliceHeader->CropUnitX * SliceHeader->SequenceParameterSet->frame_cropping_rect_left_offset +
						  SEIPanScanRectangle->pan_scan_rect_left_offset[i];
		Right                           = 16 * (16 * (SliceHeader->SequenceParameterSet->pic_width_in_mbs_minus1 + 1)  -
							SliceHeader->CropUnitX * SliceHeader->SequenceParameterSet->frame_cropping_rect_right_offset) +
						  SEIPanScanRectangle->pan_scan_rect_right_offset[i];

		Top                             = 16 * SliceHeader->CropUnitY * SliceHeader->SequenceParameterSet->frame_cropping_rect_top_offset +
						  SEIPanScanRectangle->pan_scan_rect_top_offset[i];
		Bottom                          = 16 * (16 * FrameHeightInMbs - 
							SliceHeader->CropUnitY * SliceHeader->SequenceParameterSet->frame_cropping_rect_bottom_offset) +
						  SEIPanScanRectangle->pan_scan_rect_bottom_offset[i];
#else
    // Ignoring the crop rectangle

		Left				= SEIPanScanRectangle->pan_scan_rect_left_offset[i];
		Right				= 16 * 16 * (SliceHeader->SequenceParameterSet->pic_width_in_mbs_minus1 + 1) + 
						  SEIPanScanRectangle->pan_scan_rect_right_offset[i];

		Top				= SEIPanScanRectangle->pan_scan_rect_top_offset[i];
		Bottom				= 16 * 16 * FrameHeightInMbs + 
						  SEIPanScanRectangle->pan_scan_rect_bottom_offset[i];
#endif

		PanScanState.Width[i]		= Right - Left;
		PanScanState.Height[i]		= Bottom - Top;

		PanScanState.HorizontalOffset[i]= ((Left + Right) - (16 * ParsedVideoParameters->Content.Width)) / 2;
		PanScanState.VerticalOffset[i]	= ((Top + Bottom) - (16 * ParsedVideoParameters->Content.Height)) / 2;
	    }
	}
    }

    //
    // Is pan scan on. This is recalculated every frame, because a change in policy can reset it
    //

    DisplayFormat	= Player->PolicyValue( Playback, Stream, PolicyDisplayFormat );
    DisplayAspectRatio	= Player->PolicyValue( Playback, Stream, PolicyDisplayAspectRatio );

    PanScanIsOn		= (DisplayFormat == PolicyValuePanScan) &&
			  (DisplayAspectRatio == PolicyValue4x3);

    //
    // setup the values
    //

    ParsedVideoParameters->PanScan.Count	= PanScanIsOn ? PanScanState.Count : 0;

    for( i=0; i<ParsedVideoParameters->PanScan.Count; i++ )
    {
	ParsedVideoParameters->PanScan.DisplayCount[i]		= 1;
	ParsedVideoParameters->PanScan.HorizontalOffset[i]	= PanScanState.HorizontalOffset[i];
	ParsedVideoParameters->PanScan.VerticalOffset[i]	= PanScanState.VerticalOffset[i];
    }

    //
    // Do we want to turn it off
    //

    if( PanScanState.RepetitionPeriod == 0 )
	PanScanState.Count	= 0;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to process the pan scan parameters in output order
//

void   FrameParser_VideoH264_c::CalculateCropUnits(             void )//H264SliceHeader_t               *SliceHeader )
{
    switch( SliceHeader->SequenceParameterSet->chroma_format_idc )
    {
	case 0: SliceHeader->CropUnitX          = 1;
		SliceHeader->CropUnitY          = 2 - SliceHeader->SequenceParameterSet->frame_mbs_only_flag;
		break;

	default:
	case 1: SliceHeader->CropUnitX          = 2;
		SliceHeader->CropUnitY          = 2 * (2 - SliceHeader->SequenceParameterSet->frame_mbs_only_flag);
		break;


	case 2: SliceHeader->CropUnitX          = 2;
		SliceHeader->CropUnitY          = 1 * (2 - SliceHeader->SequenceParameterSet->frame_mbs_only_flag);
		break;

	case 3: SliceHeader->CropUnitX          = 1;
		SliceHeader->CropUnitY          = 1 * (2 - SliceHeader->SequenceParameterSet->frame_mbs_only_flag);
		break;
    }

}
