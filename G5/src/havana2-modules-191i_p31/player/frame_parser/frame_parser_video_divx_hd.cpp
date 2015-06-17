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

Source file name : frame_parser_video_divx_hd.cpp
Author :           Chris

Implementation of the divx HD video frame parser class for player 2.

Date        Modification                                    Name
----        ------------                                    --------
18-Jun-06   Created                                         Chris.

************************************************************************/

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers
//
#include "frame_parser_video_divx_hd.h"

FrameParserStatus_t  FrameParser_VideoDivxHd_c::ReadVopHeader(       Mpeg4VopHeader_t         *Vop )
{
		unsigned char* ptr;
		unsigned int bits;
		FrameParserStatus_t Status;

		Status = FrameParser_VideoDivx_c::ReadVopHeader(Vop);

		if (Status == FrameParserNoError)
		{

				Bits.GetPosition(&ptr,&bits);

				if (!bits)
				{
						report( severity_error, "FrameParser_VideoDivxHd_c::ReadVopHeader - A point in the code that should never be reached - Implementation error.\n" );
						ParsedFrameParameters->DataOffset = ptr - BufferData;
						bit_skip_no = 0;
				}
				else
				{
						ParsedFrameParameters->DataOffset = ptr - BufferData;
						bit_skip_no = 8 - bits;
				}
		}

		return Status;
}

FrameParserStatus_t   FrameParser_VideoDivxHd_c::ReadHeaders( void )
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

								if (DivXVersion == 311)
								{
										unsigned char* ptr;
										Vop.prediction_type = Bits.Get(2);
										Vop.quantizer       = Bits.Get(5);
										Bits.GetPosition(&ptr,&bit_skip_no);
										ParsedFrameParameters->DataOffset = (unsigned int)(ptr - BufferData);

								}
								else
								{
										if (DivXVersion != 311 && (ParsedFrameParameters->DataOffset == 0xafff0000))
												ParsedFrameParameters->DataOffset =  ExtractStartCodeOffset(StartCodeList->StartCodes[i]);

										Status  = ReadVopHeader( &Vop );
										if( Status != FrameParserNoError )
												return Status;
								}

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
