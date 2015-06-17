/************************************************************************
Copyright (C) 2008 STMicroelectronics. All Rights Reserved.

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

Source file name : frame_parser_video_divx.cpp
Author :           Chris

Implementation of the divx video frame parser class for player 2.

Date        Modification                                    Name
----        ------------                                    --------
18-Jun-06   Created                                         Chris.

************************************************************************/

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers
//
#include "frame_parser_video_divx.h"

//#define DUMP_HEADERS 1

static BufferDataDescriptor_t     DivxStreamParametersBuffer = BUFFER_MPEG4_STREAM_PARAMETERS_TYPE;
static BufferDataDescriptor_t     DivxFrameParametersBuffer = BUFFER_MPEG4_FRAME_PARAMETERS_TYPE;

#ifdef DUMP_HEADERS
static int parsedCount = 0;
static int inputCount = 0;
static int vopCount = 0;
#endif

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//
static QuantiserMatrix_t ZigZagScan =
{
		0,  1,  8, 16,  9,  2,  3, 10,
		17, 24, 32, 25, 18, 11,  4,  5,
		12, 19, 26, 33, 40, 48, 41, 34,
		27, 20, 13,  6,  7, 14, 21, 28,
		35, 42, 49, 56, 57, 50, 43, 36,
		29, 22, 15, 23, 30, 37, 44, 51,
		58, 59, 52, 45, 38, 31, 39, 46,
		53, 60, 61, 54, 47, 55, 62, 63
};

static QuantiserMatrix_t DefaultIntraQuantizationMatrix =
{
		8,17,18,19,21,23,25,27,
		17,18,19,21,23,25,27,28,
		20,21,22,23,24,26,28,30,
		21,22,23,24,26,28,30,32,
		22,23,24,26,28,30,32,35,
		23,24,26,28,30,32,35,38,
		25,26,28,30,32,35,38,41,
		27,28,30,32,35,38,41,45
};

static QuantiserMatrix_t DefaultNonIntraQuantizationMatrix =
{
		16,17,18,19,20,21,22,23,
		17,18,19,20,21,22,23,24,
		18,19,20,21,22,23,24,25,
		19,20,21,22,23,24,26,27,
		20,21,22,23,25,26,27,28,
		21,22,23,24,26,27,28,30,
		22,23,24,26,27,28,30,31,
		23,24,25,27,28,30,31,33
};

#define PAR_SQUARE                      0x01                    /* 1:1 */
#define PAR_4_3_PAL                     0x02                    /* 12:11 */
#define PAR_4_3_NTSC                    0x03                    /* 10:11 */
#define PAR_16_9_PAL                    0x04                    /* 16:11 */
#define PAR_16_9_NTSC                   0x05                    /* 40:33 */

// BEWARE !!!! you cannot declare static initializers of a constructed type such as Rational_t
//             the compiler will silently ignore them..........
static unsigned int     DivxAspectRatioValues[][2]     =
{
		{1,1},
		{1,1},
		{12,11},
		{10,11 },
		{16,11 },
		{40,33 }
};

#define DivxAspectRatios(N) Rational_t(DivxAspectRatioValues[N][0],DivxAspectRatioValues[N][1])

//

static SliceType_t SliceTypeTranslation[]  = { SliceTypeI, SliceTypeP, SliceTypeB };

// /////////////////////////////////////////////////////////////////////////
//
//      The Constructor function
//
FrameParser_VideoDivx_c::FrameParser_VideoDivx_c( void )
{
		Configuration.FrameParserName               = "VideoDivx";

		Configuration.StreamParametersCount         = 32;
		Configuration.StreamParametersDescriptor    = &DivxStreamParametersBuffer;

		Configuration.FrameParametersCount          = 32;
		Configuration.FrameParametersDescriptor     = &DivxFrameParametersBuffer;

		//
		DivXVersion                                 = 100;

		FrameParameters  = NULL;
		StreamParameters = NULL;
		StreamParametersSet = false;

		TimeIncrementResolution                     = 1;
		prev_time_base                              = 0;
		old_time_base                               = 0;

		ReverseQueuedPostDecodeSettingsRing = NULL;
		Reset();
}

// /////////////////////////////////////////////////////////////////////////
//
//      The Destructor function
//

FrameParser_VideoDivx_c::~FrameParser_VideoDivx_c(      void )
{
#ifdef DUMP_HEADERS
		report(severity_error,"FP got %d Headers\n",inputCount);
		report(severity_error,"Parsed %d Frames\n",parsedCount);
		report(severity_error,"Parsed %d VOP Headers\n",vopCount);
#endif
		Halt();
		Reset();
}

// /////////////////////////////////////////////////////////////////////////
//
//      The Reset function release any resources, and reset all variable
//

FrameParserStatus_t   FrameParser_VideoDivx_c::Reset(   void )
{
		FrameParameters = NULL;
		FirstDecodeOfFrame = true;
		DroppedFrame = false;
		CurrentMicroSecondsPerFrame = 0;
		memset( &ReferenceFrameList, 0x00, sizeof(ReferenceFrameList_t) );

		if (StreamParameters != NULL)
		{
				delete StreamParameters;
				StreamParameters = NULL;
				SentStreamParameter = false;

		}

		memset(&LastVop,0x00,sizeof(LastVop));

		return FrameParser_Video_c::Reset();
}

// /////////////////////////////////////////////////////////////////////////
//
//      The register output ring function
//

FrameParserStatus_t   FrameParser_VideoDivx_c::RegisterOutputBufferRing(
																		Ring_t          Ring )
{
		FrameParserStatus_t     Status = FrameParserNoError;

		//report ( severity_error, "%s : %d\n",__FUNCTION__,__LINE__);

		// Clear our parameter pointers
		//
		StreamParameters                    = NULL;
		FrameParameters                     = NULL;
		DeferredParsedFrameParameters       = NULL;
		DeferredParsedVideoParameters       = NULL;

		Status = FrameParser_Video_c::RegisterOutputBufferRing( Ring );

		return Status;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Deal with decode of a single frame in forward play
//

FrameParserStatus_t   FrameParser_VideoDivx_c::ForPlayProcessFrame(      void )
{
		//      report ( severity_error, "%s : %d\n",__FUNCTION__,__LINE__);
		//
		return FrameParser_Video_c::ForPlayProcessFrame();
}

// /////////////////////////////////////////////////////////////////////////
//
//      Deal with a single frame in reverse play
//
/*
FrameParserStatus_t   FrameParser_VideoDivx_c::RevPlayProcessFrame(      void )
{
	//report ( severity_error, "%s : %d\n",__FUNCTION__,__LINE__);
	return FrameParser_Video_c::RevPlayProcessFrame();
}
*/

// /////////////////////////////////////////////////////////////////////////
//
//      Addition to the base queue a buffer for decode increments
//      the field index.
//

FrameParserStatus_t   FrameParser_VideoDivx_c::ForPlayQueueFrameForDecode( void )
{
		return  FrameParser_Video_c::ForPlayQueueFrameForDecode();
}

// /////////////////////////////////////////////////////////////////////////
//
//      Specific reverse play implementation of queue for decode
//      the field index.
//
/*
FrameParserStatus_t   FrameParser_VideoDivx_c::RevPlayQueueFrameForDecode( void )
{
	//report ( severity_error, "%s : %d\n",__FUNCTION__,__LINE__);
	return FrameParser_Video_c::RevPlayQueueFrameForDecode();
}
*/

// /////////////////////////////////////////////////////////////////////////
//
//      This function is responsible for walking the stacks to handle
//      reverse decode.
//
/*
FrameParserStatus_t   FrameParser_VideoDivx_c::RevPlayProcessDecodeStacks(              void )
{
	ReverseQueuedPostDecodeSettingsRing->Flush();
	//report ( severity_error, "%s : %d\n",__FUNCTION__,__LINE__);
	return FrameParser_Video_c::RevPlayProcessDecodeStacks();
}
*/

// /////////////////////////////////////////////////////////////////////////
//
//      This function is responsible for walking the stacks to discard
//      everything on them when we abandon reverse decode.
//
/*
FrameParserStatus_t   FrameParser_VideoDivx_c::RevPlayPurgeDecodeStacks(                void )
{
	//report ( severity_error, "%s : %d\n",__FUNCTION__,__LINE__);
	return FrameParser_Video_c::RevPlayPurgeDecodeStacks();
}
*/
FrameParserStatus_t   FrameParser_VideoDivx_c::ReadHeaders( void )
{
		unsigned int  Code;

		ParsedFrameParameters->NewStreamParameters = false;
		ParsedFrameParameters->NewFrameParameters = false;
		ParsedFrameParameters->DataOffset = 0xafff0000;

/*
	report (severity_info, "Start Code List ");
	for (unsigned int j = 0 ; j < StartCodeList->NumberOfStartCodes; ++j)
		report(severity_info,"%x ",ExtractStartCodeCode(StartCodeList->StartCodes[j]));
	report (severity_info,"\n");
*/

#ifdef DUMP_HEADERS
		++inputCount;
#endif

		//      report( severity_error, "%d start codes found\n",StartCodeList->NumberOfStartCodes);
		for(unsigned int i=0; i< StartCodeList->NumberOfStartCodes; ++i)
		{

				Code = ExtractStartCodeCode(StartCodeList->StartCodes[i]);
				//report ( severity_info, "Processing code: %x (%d of %d)\n",Code,i+1, StartCodeList->NumberOfStartCodes);
				Bits.SetPointer( BufferData + ExtractStartCodeOffset(StartCodeList->StartCodes[i]) +4);

				if ( Code == 0x31 )
				{
						// Magic to pass version number around
						DivXVersion = Bits.Get(8);
						switch (DivXVersion)
						{
						        case 3:
								        DivXVersion = 311;
								        break;
						        case 4:
								        DivXVersion = 412;
								        break;
						        case 5:
								        DivXVersion = 500;
								        break;
						        default:
								        DivXVersion = 100;
						}

						//report (severity_info, "DivX Version Number %d\n",DivXVersion);
				}
				else if (Code == DROPPED_FRAME_CODE)
				{
						// magic to work around avi's with skipped 0 byte frames inside them
						DroppedFrame = true;
						report (severity_info,"Seen AVI Dropped Frame\n");
				}
				else if( (Code & VOL_START_CODE_MASK) == VOL_START_CODE )
				{
						FrameParserStatus_t     Status = FrameParserNoError;
						// report (severity_error,"%x VOL_START_CODE\n",VOL_START_CODE);
						if( StreamParameters == NULL)
						{
								StreamParameters = new Mpeg4VideoStreamParameters_t;
								ParsedFrameParameters->StreamParameterStructure = StreamParameters;
								ParsedFrameParameters->SizeofStreamParameterStructure = sizeof(Mpeg4VideoStreamParameters_t);
								ParsedFrameParameters->NewStreamParameters = true;
						}

						Status  = ReadVolHeader( &StreamParameters->VolHeader );
						if( Status != FrameParserNoError )
						{
								ParsedFrameParameters->NewStreamParameters = false;
								StreamParametersSet = false;
								return Status;
						}

						// take some state which affects the decoding of VOP headers
						QuantPrecision = StreamParameters->VolHeader.quant_precision;
						Interlaced = StreamParameters->VolHeader.interlaced;
						StreamParameters->MicroSecondsPerFrame = CurrentMicroSecondsPerFrame;
						StreamParametersSet = true;
						//report (severity_error,"Stream is %s\n",Interlaced?"Interlaced":"Progressive");

						if (DivXVersion != 311)
								ParsedFrameParameters->DataOffset =  ExtractStartCodeOffset(StartCodeList->StartCodes[i]);

				}
				else if( (Code & VOP_START_CODE_MASK) == VOP_START_CODE )
				{
						// report (severity_error,"%x VOP_START_CODE\n",VOP_START_CODE);
						// NOTE the vop reading depends on a valid Vol having been aquired
						if( StreamParametersSet )
						{
								Mpeg4VopHeader_t Vop;
								FrameParserStatus_t     Status = FrameParserNoError;

								//                              report (severity_error,"VOP Addr %08llx\n",(BufferData + ExtractStartCodeOffset(StartCodeList->StartCodes[i]) +4));
								//
								if (DivXVersion != 311 && (ParsedFrameParameters->DataOffset == 0xafff0000))
										ParsedFrameParameters->DataOffset =  ExtractStartCodeOffset(StartCodeList->StartCodes[i]);

								Status  = ReadVopHeader( &Vop );
								if( Status != FrameParserNoError )
										return Status;

								if( FrameParameters == NULL )
								{
										FrameParserStatus_t status  = GetNewFrameParameters( (void **)&FrameParameters );
										if( status != FrameParserNoError )
										{
												report (severity_error,"Failed to get new FrameParameters\n");
												return status;
										}
								}

								FrameParameters->VopHeader = Vop;
								FrameParameters->bit_skip_no = bit_skip_no;
								ParsedFrameParameters->FrameParameterStructure = FrameParameters;
								ParsedFrameParameters->SizeofFrameParameterStructure = sizeof(Mpeg4VideoFrameParameters_t);
								ParsedFrameParameters->NewFrameParameters = true;
								Buffer->AttachBuffer( FrameParametersBuffer );
#ifdef DUMP_HEADERS
								++vopCount;
#endif
								if (DivXVersion == 311)
								{
										unsigned char* ptr;
										unsigned int bits;

										Bits.GetPosition(&ptr,&bits);
										unsigned int position = ptr - BufferData;
										if (bits) position++;
										ParsedFrameParameters->DataOffset = position;
								}

								CommitFrameForDecode();

						}
						else
						{
								report( severity_error, "Have Frame without Stream Parameters\n");
								//                              Code = INVALID_START_CODE;
						}
				}
				else if( Code == VSOS_START_CODE )
				{
						// report (severity_error,"%x VSOS_START_CODE\n",VSOS_START_CODE);
						//
						// NOTE this could be my AVI header added to send the MS per frame from the
						// AVI file
						// some avi files have real ones which upset things.... if it returns back 0
						// which is from an unknown profile then use the last one, otherwise pick 25fps
						unsigned int cnpf =  ReadVosHeader( );

						if ( cnpf != 0 )
								CurrentMicroSecondsPerFrame = cnpf;

						// Nick changed this to sanitize the frame rate (4..120)
						if ( (CurrentMicroSecondsPerFrame == 0) || !inrange(Rational_t(1000000,CurrentMicroSecondsPerFrame), 4, 120) )
						{
								report(severity_error,"Current ms per frame not valid (%d) defaulting to 25fps\n");
								CurrentMicroSecondsPerFrame = 40000;
						}
						//report (severity_error,"CurrentMicroSecondsPerFrame %d\n",CurrentMicroSecondsPerFrame);
				}
				else if( (Code & VO_START_CODE_MASK) == VO_START_CODE )
				{
						report (severity_info,"%x VO_START_CODE\n",VO_START_CODE);
						ReadVoHeader();
				}
/*
				else if( (Code & USER_DATA_START_CODE_MASK) == USER_DATA_START_CODE )
				{
						report (severity_info,"%x USER_DATA_START_CODE\n",USER_DATA_START_CODE);
				} // No action
				else if( Code == VSOS_END_CODE )
				{
                      report (severity_info,"%x VSOS_END_CODE\n",VSOS_END_CODE);
				} // No action
				else if( Code == VSO_START_CODE )
				{
						report (severity_info,"%x VSO_START_CODE\n",VSO_START_CODE);
				} // No action
				else if( Code == GOV_START_CODE )
				{
						report (severity_info,"%x GOV_START_CODE\n",GOV_START_CODE);
				} // No action
				else if( Code == END_OF_START_CODE_LIST )
				{
						report (severity_error,"%x END_OF_START_CODE_LIST\n",END_OF_START_CODE_LIST);
				} // No action
				else
				{
						report( severity_error, "ReadHeaders - Unknown/Unsupported header 0x%02x\n", Code );
				}
*/
		}

		return FrameParserNoError;
}

FrameParserStatus_t   FrameParser_VideoDivx_c::CommitFrameForDecode(void)
{
		SliceType_t		 SliceType;
		Mpeg4VideoFrameParameters_t* VFP = (Mpeg4VideoFrameParameters_t*)(ParsedFrameParameters->FrameParameterStructure);
		Mpeg4VopHeader_t*  Vop = &(VFP->VopHeader);

#ifdef DUMP_HEADERS
		++parsedCount;
#endif

		if (Vop->divx_nvop || (StreamParameters == NULL) )
		{
				if (Vop->vop_coded)
						report(severity_error,"%s: can't be divx_nvop and vop_coded! (%p)\n",__FUNCTION__, StreamParameters);

				if (DivXVersion == 311)
				{
						report(severity_error,"%s: can't be divx_nvop in a faked DivX3.11 frame?\n",__FUNCTION__);
						Player->MarkStreamUnPlayable( Stream );
				}
				return FrameParserError;
		}

		SliceType		= SliceTypeTranslation[Vop->prediction_type];

		ParsedFrameParameters->SizeofStreamParameterStructure = sizeof(Mpeg4VideoStreamParameters_t);
		ParsedFrameParameters->StreamParameterStructure = StreamParameters;

		if (StreamParameters->VolHeader.aspect_ratio_info <= PAR_16_9_NTSC)
				ParsedVideoParameters->Content.PixelAspectRatio =
				DivxAspectRatios(StreamParameters->VolHeader.aspect_ratio_info);
		else if (StreamParameters->VolHeader.aspect_ratio_info == PAR_EXTENDED)
				ParsedVideoParameters->Content.PixelAspectRatio =
				Rational_t(StreamParameters->VolHeader.par_width, StreamParameters->VolHeader.par_height);
		else
				ParsedVideoParameters->Content.PixelAspectRatio =
				DivxAspectRatios(PAR_SQUARE);

		ParsedVideoParameters->Content.Width = StreamParameters->VolHeader.width;
		ParsedVideoParameters->Content.Height = StreamParameters->VolHeader.height;
		ParsedVideoParameters->Content.DisplayWidth = StreamParameters->VolHeader.width;
		ParsedVideoParameters->Content.DisplayHeight = StreamParameters->VolHeader.height;
		ParsedVideoParameters->Content.Progressive = !Interlaced;
		ParsedVideoParameters->Content.FrameRate = Rational_t(1000000,StreamParameters->MicroSecondsPerFrame);
		ParsedVideoParameters->Content.OverscanAppropriate = 0;
		ParsedVideoParameters->InterlacedFrame = Interlaced;

		ParsedFrameParameters->FirstParsedParametersForOutputFrame = true;
		ParsedFrameParameters->FirstParsedParametersAfterInputJump = FirstDecodeAfterInputJump;
		ParsedFrameParameters->SurplusDataInjected = SurplusDataInjected;
		ParsedFrameParameters->ContinuousReverseJump = ContinuousReverseJump;
		ParsedFrameParameters->KeyFrame		= SliceType == SliceTypeI;
		ParsedFrameParameters->ReferenceFrame	= SliceType != SliceTypeB;
		ParsedFrameParameters->IndependentFrame	= ParsedFrameParameters->KeyFrame;
		ParsedFrameParameters->NumberOfReferenceFrameLists = 1;

		ParsedVideoParameters->DisplayCount[0] = 1 + (DroppedFrame * 1);
		if (Interlaced)
				ParsedVideoParameters->DisplayCount[1] = 1 + (DroppedFrame * 1);
		else
				ParsedVideoParameters->DisplayCount[1] = 0;

		ParsedVideoParameters->SliceType = SliceType;
		ParsedVideoParameters->TopFieldFirst = Vop->top_field_first;
		ParsedVideoParameters->PictureStructure = StructureFrame;

		ParsedVideoParameters->PanScan.Count                            = 1;
		ParsedVideoParameters->PanScan.DisplayCount[0]                  = ParsedVideoParameters->DisplayCount[0] + ParsedVideoParameters->DisplayCount[1];
		ParsedVideoParameters->PanScan.HorizontalOffset[0]              = 0;
		ParsedVideoParameters->PanScan.VerticalOffset[0]                = 0;

		FirstDecodeOfFrame = true;
		FrameToDecode = true;
		FrameParameters = NULL;
		DroppedFrame = false;

		return FrameParserNoError;
}

FrameParserStatus_t  FrameParser_VideoDivx_c::ReadVoHeader(void)
{
		unsigned int IsVisualObjectIdentifier;
		unsigned int VisualObjectVerId;
		unsigned int VisualObjectPriority;
		unsigned int VisualObjectType;
		unsigned int VideoSignalType;
		unsigned int VideoFormat;
		unsigned int VideoRange;
		unsigned int ColourPrimaries;
		unsigned int TransferCharacteristics;
		unsigned int MatrixCoefficients;

		IsVisualObjectIdentifier = Bits.Get(1);
		if (IsVisualObjectIdentifier)
		{
				VisualObjectVerId = Bits.Get(4);
				VisualObjectPriority = Bits.Get(3);
		}
		else
				VisualObjectVerId = 1;

		VisualObjectType = Bits.Get(4);

		if (VisualObjectType == 1) // Video ID
		{
				VideoSignalType = Bits.Get(1);
				if (VideoSignalType)
				{
						unsigned char ColourDescription;
						VideoFormat = Bits.Get(3);
						VideoRange = Bits.Get(1);
						ColourDescription = Bits.Get(1);
						if (ColourDescription)
						{
								ColourPrimaries = Bits.Get(8);
								TransferCharacteristics = Bits.Get(8);
								MatrixCoefficients = Bits.Get(8);
						}
						else
						{
								ColourPrimaries = 1;   /*COLOUR_PRIMARIES_ITU_R_BT_709 */
								TransferCharacteristics = 1;   /*TRANSFER_ITU_R_BT_709 */
								MatrixCoefficients = 1;        /*MATRIX_COEFFICIENTS_ITU_R_BT_709 */
						}

						switch (MatrixCoefficients)
						{
						        case MPEG4P2_MATRIX_COEFFICIENTS_ITU_R_BT_709 :
								        MatrixCoefficients = MatrixCoefficients_ITU_R_BT709; break;
						        case MPEG4P2_MATRIX_COEFFICIENTS_FCC :
								        MatrixCoefficients= MatrixCoefficients_FCC; break;
						        case MPEG4P2_MATRIX_COEFFICIENTS_ITU_R_BT_470_2_BG :
								        MatrixCoefficients= MatrixCoefficients_ITU_R_BT470_2_BG; break;
						        case MPEG4P2_MATRIX_COEFFICIENTS_SMPTE_170M :
								        MatrixCoefficients= MatrixCoefficients_SMPTE_170M; break;
						        case MPEG4P2_MATRIX_COEFFICIENTS_SMPTE_240M :
								        MatrixCoefficients= MatrixCoefficients_SMPTE_240M; break;
						        default:
						        case MPEG4P2_MATRIX_COEFFICIENTS_UNSPECIFIED :
								        MatrixCoefficients= MatrixCoefficients_Undefined; break;
						}

						ParsedVideoParameters->Content.ColourMatrixCoefficients = MatrixCoefficients;
						ParsedVideoParameters->Content.VideoFullRange = false; // not used by DivX

						report(severity_info, "Colour Primaries %d\n",ColourPrimaries);
						report(severity_info, "Transfer Characteristics %d\n",TransferCharacteristics);
						report(severity_info, "Matrix Coefficients %d\n",MatrixCoefficients);

				}

		}

		return FrameParserNoError;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in the vol header.
//

FrameParserStatus_t  FrameParser_VideoDivx_c::ReadVolHeader( Mpeg4VolHeader_t       *Vol )
{
		int i;

		memset( Vol, 0, sizeof(Mpeg4VolHeader_t) );

		//

		Vol->random_accessible_vol                  = Bits.Get(1);
		Vol->type_indication                        = Bits.Get(8);
		Vol->is_object_layer_identifier             = Bits.Get(1);

		if( Vol->is_object_layer_identifier )
		{
				Vol->visual_object_layer_verid          = Bits.Get(4);
				Vol->visual_object_layer_priority       = Bits.Get(3);
		}
		else
		{
				Vol->visual_object_layer_verid          = 1;
				Vol->visual_object_layer_priority       = 1;
		}

		Vol->aspect_ratio_info                      = Bits.Get(4);
		if( Vol->aspect_ratio_info == PAR_EXTENDED )
		{
				Vol->par_width                          = Bits.Get(8);
				Vol->par_height                         = Bits.Get(8);
		}

		Vol->vol_control_parameters                 = Bits.Get(1);
		if( Vol->vol_control_parameters )
		{
				Vol->chroma_format                      = Bits.Get(2);
				Vol->low_delay                          = Bits.Get(1);
				Vol->vbv_parameters                     = Bits.Get(1);
				if( Vol->vbv_parameters )
				{
						Vol->first_half_bit_rate            = Bits.Get(15);
						Bits.Get(1);
						Vol->latter_half_bit_rate           = Bits.Get(15);
						Bits.Get(1);
						Vol->first_half_vbv_buffer_size     = Bits.Get(15);
						Bits.Get(1);
						Vol->latter_half_vbv_buffer_size    = Bits.Get(3);
						Vol->first_half_vbv_occupancy       = Bits.Get(11);
						Bits.Get(1);
						Vol->latter_half_vbv_occupancy      = Bits.Get(15);
						Bits.Get(1);
				}
		}

		Vol->shape                                  = Bits.Get(2);

		if( Vol->shape != SHAPE_RECTANGULAR )
		{
				report( severity_error, "Frame_Mpeg4Video_c::ReadVolHeader - ERROR **** VolHeader shape other than RECTANGULAR not supported\n" );
				//Player->MarkStreamUnPlayable( Stream );
				return FrameParserError;
		}

		// NICK ... The following code is adjusted to only cater for shape == RECTANGULAR
		//
		Bits.Get(1);

		Vol->time_increment_resolution              = Bits.Get(16);
		TimeIncrementBits                           = INCREMENT_BITS(Vol->time_increment_resolution);
		TimeIncrementResolution                     = Vol->time_increment_resolution;

		Bits.Get(1);
		Vol->fixed_vop_rate                         = Bits.Get(1);
		if( Vol->fixed_vop_rate )
				Vol->fixed_vop_time_increment           = Bits.Get(TimeIncrementBits);

		Bits.Get(1);
		Vol->width                                  = Bits.Get(13);
		Bits.Get(1);
		Vol->height                                 = Bits.Get(13);
		Bits.Get(1);

		Vol->version = DivXVersion;

		Vol->interlaced                             = Bits.Get(1);
		Vol->obmc_disable                           = Bits.Get(1);

		if( Vol->visual_object_layer_verid == 1 )
				Vol->sprite_usage                       = Bits.Get(1);
		else
				Vol->sprite_usage                       = Bits.Get(2);

		if( Vol->sprite_usage != SPRITE_NOT_USED )
		{
				report( severity_error, "Frame_Mpeg4Video_c::ReadVolHeader - ERROR **** VolHeader sprite_usage other than NOT_USED not supported.\n" );
				Player->MarkStreamUnPlayable( Stream );
				return FrameParserError;

				// code to support GMC assuming firmware is compiled with GMC support.
				//
				if(Vol->sprite_usage != GMC_SPRITE)
				{

						Bits.Get(13 + 1 + 13 + 1);
						// sprite_width (13 bits)
						// marker (1 bit)
						// sprite_height (13 bits)
						// marker (1 bit)
						Bits.Get(13 + 1 + 13 + 1);
						// sprite_left_coordinate (13 bits)
						// marker (1 bit)
						// sprite_top_coordinate (13 bits)
						// marker (1 bit)
				}

				Vol->no_of_sprite_warping_points = Bits.Get(6);
				Vol->sprite_warping_accuracy = Bits.Get(2);
				// 00: halfpel
				// 01: 1/4 pel
				// 10: 1/8 pel
				// 11: 1/16 pel
				//
				Vol->sprite_brightness_change=Bits.Get(1);
				//       assert(!Vol->sprite_brightness_change);
				if(Vol->sprite_usage != GMC_SPRITE)
				{
						//           int low_latency_sprite_enable=
						Bits.Get(1);
				}
		}

		// NICK ... The following code is adjusted to only cater for sprite_usage == SPRITE_NOT_USED
		//
		Vol->not_8_bit                              = Bits.Get(1);
		if( Vol->not_8_bit )
		{
				Vol->quant_precision                    = Bits.Get(4);
				Vol->bits_per_pixel                     = Bits.Get(4);
		}
		else
		{
				Vol->quant_precision                    = 5;
				Vol->bits_per_pixel                     = 8;
		}

		Vol->quant_type                             = Bits.Get(1);
		if( Vol->quant_type )
		{
				Vol->load_intra_quant_matrix            = Bits.Get(1);
				if( Vol->load_intra_quant_matrix )
				{
						for( i=0; i<QUANTISER_MATRIX_SIZE; i++ )
						{
								Vol->intra_quant_matrix[ZigZagScan[i]] = Bits.Get(8);
								if( Vol->intra_quant_matrix[ZigZagScan[i]] == 0 )
										break;
						}
						for( ; i<QUANTISER_MATRIX_SIZE; i++ )
								Vol->intra_quant_matrix[ZigZagScan[i]] = Vol->intra_quant_matrix[ZigZagScan[i-1]];
				}
				else
						memcpy( Vol->intra_quant_matrix, DefaultIntraQuantizationMatrix, QUANTISER_MATRIX_SIZE );

				Vol->load_non_intra_quant_matrix        = Bits.Get(1);
				if( Vol->load_non_intra_quant_matrix )
				{
						for( i=0; i<QUANTISER_MATRIX_SIZE; i++ )
						{
								Vol->non_intra_quant_matrix[ZigZagScan[i]] = Bits.Get(8);
								if( Vol->non_intra_quant_matrix[ZigZagScan[i]] == 0 )
										break;
						}
						for( ; i<QUANTISER_MATRIX_SIZE; i++ )
								Vol->non_intra_quant_matrix[ZigZagScan[i]] = Vol->non_intra_quant_matrix[ZigZagScan[i-1]];
				}
				else
						memcpy( Vol->non_intra_quant_matrix, DefaultNonIntraQuantizationMatrix, QUANTISER_MATRIX_SIZE );
		}

		if( Vol->visual_object_layer_verid != 1 )
				Vol->quarter_pixel                      = Bits.Get(1);
		else
				Vol->quarter_pixel                      = 0;

		if( Vol->quarter_pixel )
		{
				report( severity_error, "Frame_Mpeg4Video_c::ReadVolHeader - ERROR **** VolHeader quarter_pixel not supported.\n" );
				Player->MarkStreamUnPlayable( Stream );
				return FrameParserError;
		}

		Vol->complexity_estimation_disable          = Bits.Get(1);
		Vol->resync_marker_disable                  = Bits.Get(1);
		Vol->data_partitioning                      = Bits.Get(1);
		if (Vol->data_partitioning)
				Vol->reversible_vlc                     = Bits.Get(1);

		if( Vol->visual_object_layer_verid != 1 )
		{
				Vol->intra_acdc_pred_disable            = Bits.Get(1);
				if( Vol->intra_acdc_pred_disable )
				{
						Vol->request_upstream_message_type  = Bits.Get(2);
						Vol->newpred_segment_type           = Bits.Get(1);
				}
				Vol->reduced_resolution_vop_enable      = Bits.Get(1);
		}

		Vol->scalability                            = Bits.Get(1);
		if( Vol->scalability )
		{
				report( severity_error, "Frame_Mpeg4Video_c::ReadVolHeader - ERROR **** VolHeader scalability not supported.\n" );
				//Player->MarkStreamUnPlayable( Stream );
				return FrameParserError;
		}

		//
		if( !Vol->complexity_estimation_disable )
		{
				report( severity_error, "Frame_Mpeg4Video_c::ReadVolHeader - ERROR **** VolHeader complexity_estimation_disable not set\n" );
				//Player->MarkStreamUnPlayable( Stream );
				return FrameParserError;
		}

		if( Vol->data_partitioning )
		{
				report( severity_error, "Frame_Mpeg4Video_c::ReadVolHeader - ERROR **** VolHeader data_partitioning not supported.\n" );
				//Player->MarkStreamUnPlayable( Stream );
				return FrameParserError;
		}

#if 0
		report( severity_info, "VOL header update - time_increment_resolution = %d, fixed_vop_rate = %d, fixed_vop_time_increment = %d\n", Vol->time_increment_resolution, Vol->fixed_vop_rate, Vol->fixed_vop_time_increment);
#endif
		//
#ifdef DUMP_HEADERS
		report( severity_info, "Vol header :- \n" );
		report( severity_info, "        random_accessible_vol             %6d\n", Vol->random_accessible_vol );
		report( severity_info, "        type_indication                   %6d\n", Vol->type_indication );
		report( severity_info, "        is_object_layer_identifier        %6d\n", Vol->is_object_layer_identifier );
		report( severity_info, "        visual_object_layer_verid         %6d\n", Vol->visual_object_layer_verid );
		report( severity_info, "        visual_object_layer_priority      %6d\n", Vol->visual_object_layer_priority );
		report( severity_info, "        aspect_ratio_info                 %6d\n", Vol->aspect_ratio_info );
		report( severity_info, "        par_width                         %6d\n", Vol->par_width );
		report( severity_info, "        par_height                        %6d\n", Vol->par_height );
		report( severity_info, "        vol_control_parameters            %6d\n", Vol->vol_control_parameters );
		report( severity_info, "        chroma_format                     %6d\n", Vol->chroma_format );
		report( severity_info, "        low_delay                         %6d\n", Vol->low_delay );
		report( severity_info, "        vbv_parameters                    %6d\n", Vol->vbv_parameters );
		report( severity_info, "        first_half_bit_rate               %6d\n", Vol->first_half_bit_rate );
		report( severity_info, "        latter_half_bit_rate              %6d\n", Vol->latter_half_bit_rate );
		report( severity_info, "        first_half_vbv_buffer_size        %6d\n", Vol->first_half_vbv_buffer_size );
		report( severity_info, "        latter_half_vbv_buffer_size       %6d\n", Vol->latter_half_vbv_buffer_size );
		report( severity_info, "        first_half_vbv_occupancy          %6d\n", Vol->first_half_vbv_occupancy );
		report( severity_info, "        latter_half_vbv_occupancy         %6d\n", Vol->latter_half_vbv_occupancy );
		report( severity_info, "        shape                             %6d\n", Vol->shape );
		report( severity_info, "        time_increment_resolution         %6d\n", Vol->time_increment_resolution );
		report( severity_info, "        fixed_vop_rate                    %6d\n", Vol->fixed_vop_rate );
		report( severity_info, "        fixed_vop_time_increment          %6d\n", Vol->fixed_vop_time_increment );
		report( severity_info, "        width                             %6d\n", Vol->width );
		report( severity_info, "        height                            %6d\n", Vol->height );
		report( severity_info, "        interlaced                        %6d\n", Vol->interlaced );
		report( severity_info, "        obmc_disable                      %6d\n", Vol->obmc_disable );
		report( severity_info, "        sprite_usage                      %6d\n", Vol->sprite_usage );
		report( severity_info, "        not_8_bit                         %6d\n", Vol->not_8_bit );
		report( severity_info, "        quant_precision                   %6d\n", Vol->quant_precision );
		report( severity_info, "        bits_per_pixel                    %6d\n", Vol->bits_per_pixel );
		report( severity_info, "        quant_type                        %6d\n", Vol->quant_type );
		report( severity_info, "        load_intra_quant_matrix           %6d\n", Vol->load_intra_quant_matrix );
		report( severity_info, "        load_non_intra_quant_matrix       %6d\n", Vol->load_non_intra_quant_matrix );
		report( severity_info, "        quarter_pixel                     %6d\n", Vol->quarter_pixel );
		report( severity_info, "        complexity_estimation_disable     %6d\n", Vol->complexity_estimation_disable );
		//report( severity_info, "        error_res_disable                 %6d\n", Vol->error_res_disable );
		report( severity_info, "        data_partitioning                 %6d\n", Vol->data_partitioning );
		report( severity_info, "        intra_acdc_pred_disable           %6d\n", Vol->intra_acdc_pred_disable );
		report( severity_info, "        request_upstream_message_type     %6d\n", Vol->request_upstream_message_type );
		report( severity_info, "        newpred_segment_type              %6d\n", Vol->newpred_segment_type );
		report( severity_info, "        reduced_resolution_vop_enable     %6d\n", Vol->reduced_resolution_vop_enable );
		report( severity_info, "        scalability                       %6d\n", Vol->scalability );
#endif

		return FrameParserNoError;

}

static unsigned int divround1(int v1, int v2)
{
	    return (v1+v2/2)/v2;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in a Vop header
//

FrameParserStatus_t  FrameParser_VideoDivx_c::ReadVopHeader(       Mpeg4VopHeader_t         *Vop )
{
		unsigned int    display_time = 0;
		//

		memset( Vop, 0, sizeof(Mpeg4VopHeader_t) );

		// Not sure this is true now!
		// NOTE this code assumes that VolHeader        shape           == RECTANGULAR
		//                                              sprite_usage    == SPRITE_NOT_USED
		//

		Vop->prediction_type                        = Bits.Get(2);

		if(Vop->prediction_type != PREDICTION_TYPE_B)
		{
				prev_time_base = old_time_base;
				display_time = old_time_base;
		}
		else
				display_time = prev_time_base;

		while(Bits.Get(1) == 1)
		{
				if (Vop->prediction_type != PREDICTION_TYPE_B)
						Vop->time_base++;
				display_time++;
		}

		old_time_base += Vop->time_base;

		Bits.Get(1);
		Vop->time_inc                               = Bits.Get(TimeIncrementBits);
		Bits.Get(1);

		// trb/trd - display_time calculation
		//
		display_time = display_time * TimeIncrementResolution + Vop->time_inc;
		if (Vop->prediction_type != PREDICTION_TYPE_B)
		{
				LastVop.display_time_prev       = LastVop.display_time_next;
				LastVop.display_time_next       = display_time;

				if (display_time != LastVop.display_time_prev)
				{
						Vop->trd        = LastVop.display_time_next - LastVop.display_time_prev;
						LastVop.trd     = Vop->trd;
				}

		/* to make the code match the firmware test env */
				Vop->trb = LastVop.trb;
				Vop->trb_trd = LastVop.trb_trd;
				Vop->trb_trd_trd = LastVop.trb_trd_trd;

		}
		else
		{
				Vop->trd        = LastVop.trd;
				Vop->trb        = display_time - LastVop.display_time_prev;
				if(Interlaced)
				{
						if(Vop->tframe == -1)
								Vop->tframe = LastVop.display_time_next - display_time;
						Vop->trbi=
								2*(divround1(display_time,Vop->tframe) - divround1(LastVop.display_time_prev, Vop->tframe));
						Vop->trdi=
								2*(divround1(LastVop.display_time_next,Vop->tframe) - divround1(LastVop.display_time_prev, Vop->tframe));
				}

		/* set up the frame values for trb_trd and trb_trd_trd */

				if( LastVop.trd != 0 )
				{
					Vop->trb_trd = ((Vop->trb << 16) / LastVop.trd) + 1;
					Vop->trb_trd_trd = (((Vop->trb - LastVop.trd) << 16) / LastVop.trd) - 1;
				}
		}

		LastVop.trb = Vop->trb;
		LastVop.trd = Vop->trd;
		LastVop.trb_trd = Vop->trb_trd;
		LastVop.trb_trd_trd = Vop->trb_trd_trd;

		Vop->vop_coded                              = Bits.Get(1);

		if( Vop->vop_coded )
		{
				if( Vop->prediction_type == PREDICTION_TYPE_P )
				{
						Vop->rounding_type                  = Bits.Get(1);
				}
				else
						Vop->rounding_type                  = 0;

				Vop->intra_dc_vlc_thr                   = Bits.Get(3);
				if(Interlaced)
				{
						Vop->top_field_first                = Bits.Get(1);
						Vop->alternate_vertical_scan_flag   = Bits.Get(1);
#if 0
						report(severity_error, "interlaced info : tff %d, avsf %d\n",
							   Vop->top_field_first,
							   Vop->alternate_vertical_scan_flag);
#endif
						// 25fps is a frame rate of 40ms per frame
						if ((CurrentMicroSecondsPerFrame == 40000) && (Vop->top_field_first == 0))
						{
#if 0
								report(severity_error, "PAL is normally top_field_first.. inverting\n");
#endif
								Vop->top_field_first = 1;
						}
				}
				else
				{
						Vop->top_field_first                = 0;
						Vop->alternate_vertical_scan_flag   = 0;
				}
				Vop->quantizer                          = Bits.Get(QuantPrecision);

				if( Vop->prediction_type != PREDICTION_TYPE_I )
						Vop->fcode_forward                  = Bits.Get(3);

				Vop->fcode_backward = LastVop.fcode_backward; // seems to match what the firmware test app does
				if( Vop->prediction_type == PREDICTION_TYPE_B )
						Vop->fcode_backward                 = Bits.Get(3);
				LastVop.fcode_backward = Vop->fcode_backward; // seems to match what the firmware test app does

		}
		else
		{
				// try to determin a fake DivX NVOP or a real NVOP
				if( ( Vop->prediction_type == PREDICTION_TYPE_P ) &&
					( LastPredictionType != PREDICTION_TYPE_B ) &&
					( LastTimeIncrement == Vop->time_inc) )
				{
#if 0
						report(severity_info, "DivX NVOP detected %d %d %d %d\n",
							   Vop->prediction_type,
							   LastPredictionType,
							   LastTimeIncrement,
							   Vop->time_inc);
#endif
						Vop->divx_nvop = 1;
				}
#if 0
				else
				{
						report(severity_info, "real NVOP detected %d %d %d %d\n",
							   Vop->prediction_type,
							   LastPredictionType,
							   LastTimeIncrement,
							   Vop->time_inc);
				}
#endif
		}

		if (Vop->prediction_type != PREDICTION_TYPE_B)
		{
				LastPredictionType = Vop->prediction_type;
				LastTimeIncrement = Vop->time_inc;
		}
		//

#ifdef DUMP_HEADERS
		report( severity_info, "Vop header :- \n" );
		report( severity_info, "        prediction_type                   %6d\n", Vop->prediction_type );
		report( severity_info, "        quantizer                         %6d\n", Vop->quantizer );
		report( severity_info, "        rounding_type                     %6d\n", Vop->rounding_type );
		report( severity_info, "        fcode_for                         %6d\n", Vop->fcode_forward );
		report( severity_info, "        vop_coded                         %6d\n", Vop->vop_coded );
		report( severity_info, "        use_intra_dc_vlc                  %6d\n", Vop->intra_dc_vlc_thr?0:1 );
		report( severity_info, "        trbi                              %6d\n", Vop->trbi );
		report( severity_info, "        trdi                              %6d\n", Vop->trdi );
		report( severity_info, "        trb_trd                           %6d\n", Vop->trb_trd );
		report( severity_info, "        trb_trd_trd                       %6d\n", Vop->trb_trd_trd );
		report( severity_info, "        trd                               %6d\n", Vop->trd );
		report( severity_info, "        trb                               %6d\n", Vop->trb );
		report( severity_info, "        intra_dc_vlc_thr                  %6d\n", Vop->intra_dc_vlc_thr );
		report( severity_info, "        time_inc                          %6d\n", Vop->time_inc );
		report( severity_info, "        fcode_back                        %6d\n", Vop->fcode_backward );
		report( severity_info, "        shape_coding_type                 %6d\n", 0);
		report( severity_info, "        bit_skip_no                       %6d\n", bit_skip_no );
		report( severity_info, "        time_base                         %6d\n", Vop->time_base );
		report( severity_info, "        rounding_type                     %6d\n", Vop->rounding_type );

#endif
/*
    report( severity_info, "        prediction_type is %d\n", Vop->prediction_type );
    report( severity_info, "        quantizer is %d\n", Vop->quantizer );
    report( severity_info, "        rounding_type is %d\n", Vop->rounding_type );
    report( severity_info, "        fcode_for is %d\n", Vop->fcode_forward );
    report( severity_info, "        vop_coded is %d\n", Vop->vop_coded );
    report( severity_info, "        use_intra_dc_vlc is %d\n", Vop->intra_dc_vlc_thr?0:1 );
    report( severity_info, "        trbi is %d\n", Vop->trbi );
    report( severity_info, "        trdi is %d\n", Vop->trdi );
    report( severity_info, "        trb_trd is %d\n", Vop->trb_trd );
    report( severity_info, "        trb_trd_trd is %d\n", Vop->trb_trd_trd );
    report( severity_info, "        trd is %d\n", Vop->trd );
    report( severity_info, "        trb is %d\n", Vop->trb );
    report( severity_info, "        intra_dc_vlc_thr is %d\n", Vop->intra_dc_vlc_thr );
    report( severity_info, "        time_inc is %d\n", Vop->time_inc );
    report( severity_info, "        fcode_back is %d\n", Vop->fcode_backward );
    report( severity_info, "        shape_coding_type is %d\n", 0);
    report( severity_info, "        bit_skip_no is %d\n", bit_skip_no );
*/
		return FrameParserNoError;

}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in the vos header.
//
unsigned int FrameParser_VideoDivx_c::ReadVosHeader( )
{
		unsigned int profile = Bits.Get(8);
		if (profile != 0)
		{
				//report(severity_error, "bad Vos header - profile = %d\n", profile);
				return 0;         /* STTC has a profile set to 0 which is Reserved in the mpeg4 spec */
		}

		if (Bits.Get(32) == 0x1b2)
		{
				if (Bits.Get(32) == 0x53545443)
						return Bits.Get(32);
		}
		//report(severity_error, "bad Vos header\n");
		return 0;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to prepare a reference frame list
//

FrameParserStatus_t   FrameParser_VideoDivx_c::PrepareReferenceFrameList( void )
{
		unsigned int    i;
		unsigned int    ReferenceFramesNeeded;

		//
		// Note we cannot use StreamParameters or FrameParameters to address data directly,
		// as these may no longer apply to the frame we are dealing with.
		// Particularly if we have seen a sequenece header or group of pictures
		// header which belong to the next frame.
		//

		ReferenceFramesNeeded = (( Mpeg4VideoFrameParameters_t *)(ParsedFrameParameters->FrameParameterStructure))->VopHeader.prediction_type;

		if( ReferenceFrameList.EntryCount < ReferenceFramesNeeded )
		{
				report(severity_error,"Insufficient Ref Frames %d vs %d\n", ReferenceFrameList.EntryCount,  ReferenceFramesNeeded);
				return FrameParserInsufficientReferenceFrames;
		}

		ParsedFrameParameters->NumberOfReferenceFrameLists                  = 1;
		ParsedFrameParameters->ReferenceFrameList[0].EntryCount             = ReferenceFramesNeeded;
		for( i=0; i<ReferenceFramesNeeded; i++ )
		{
				ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[i]   =
						ReferenceFrameList.EntryIndicies[ReferenceFrameList.EntryCount - ReferenceFramesNeeded + i];
		}

		return FrameParserNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to manage a reference frame list in forward play
//      we only record a reference frame as such on the last field, in order to
//      ensure the correct management of reference frames in the codec, we immediately
//      inform the codec of a release on the first field of a field picture.
//

FrameParserStatus_t   FrameParser_VideoDivx_c::ForPlayUpdateReferenceFrameList( void )
{

		if( ParsedFrameParameters->ReferenceFrame )
		{

				if( ReferenceFrameList.EntryCount >= MAX_REFERENCE_FRAMES_FORWARD_PLAY )
				{
						Player->CallInSequence( Stream, SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, ReferenceFrameList.EntryIndicies[0] );

						ReferenceFrameList.EntryCount--;
						for( unsigned int i=0; i<ReferenceFrameList.EntryCount; i++ )
								ReferenceFrameList.EntryIndicies[i] = ReferenceFrameList.EntryIndicies[i+1];
				}

				ReferenceFrameList.EntryIndicies[ReferenceFrameList.EntryCount++] = ParsedFrameParameters->DecodeFrameIndex;

		}

		return FrameParserNoError;
}
