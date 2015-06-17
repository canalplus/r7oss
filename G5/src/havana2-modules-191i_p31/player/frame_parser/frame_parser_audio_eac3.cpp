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

Source file name : frame_parser_audio_eac3.cpp
Author :           Sylvain

Implementation of the EAC3 audio frame parser class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
22-May-07   Created (from frame_parser_audio_ac3.cpp)      Sylvain

************************************************************************/

////////////////////////////////////////////////////////////////////////
///
/// \class FrameParser_AudioEAc3
/// \brief Frame parser for EAC3 audio
///

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "eac3_audio.h"
#include "frame_parser_audio_eac3.h"

//
#ifdef ENABLE_FRAME_DEBUG
#undef ENABLE_FRAME_DEBUG
#define ENABLE_FRAME_DEBUG 0
#endif


// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

#undef FRAME_TAG
#define FRAME_TAG "EAC3 audio frame parser"

static BufferDataDescriptor_t     EAc3AudioStreamParametersBuffer = BUFFER_EAC3_AUDIO_STREAM_PARAMETERS_TYPE;
static BufferDataDescriptor_t     EAc3AudioFrameParametersBuffer = BUFFER_EAC3_AUDIO_FRAME_PARAMETERS_TYPE;

//
// Lookup tables for EAC3 header parsing
//

static int AC3Rate[] =
{
    32,  40,  48,  56,  64,  80,  96, 112,
    128, 160, 192, 224, 256, 320, 384, 448,
    512, 576, 640
};

static int EAC3Blocks[] = 
{
	1, 2, 3, 6
};

static char EAC3FsCodeToFrameSize[] = 
{
	2, 0, 3
};
	
static int EAC3SamplingFreq[] = 
{
	48000, 44100, 32000, 0
};


// /////////////////////////////////////////////////////////////////////////
//
// Locally defined macros
//


// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

// /////////////////////////////////////////////////////////////////////////
///
/// This function parses an independant frame header and the 
/// possible following dependant eac3 frame headers
///
/// \return Frame parser status code, FrameParserNoError indicates success.

FrameParserStatus_t FrameParser_AudioEAc3_c::ParseFrameHeader( unsigned char *FrameHeaderBytes, 
															   EAc3AudioParsedFrameHeader_t *ParsedFrameHeader,
															   int GivenFrameSize )
{
	int StreamIndex =  0, FrameSize =  0 ;
	EAc3AudioParsedFrameHeader_t NextParsedFrameHeader;
	int NumberOfIndependantSubStreams = 0;
	int NumberOfDependantSubStreams = 0;
	int NumberOfSamples = 0;
	
	memset(ParsedFrameHeader, 0, sizeof(EAc3AudioParsedFrameHeader_t));
	
	do 
	{
		// At this point we got the idependant substream or stream length, now search for 
		// the other dependant substreams
		unsigned char* NextFrameHeader = &FrameHeaderBytes[StreamIndex];
		FrameParserStatus_t Status;
		memset(&NextParsedFrameHeader, 0, sizeof(EAc3AudioParsedFrameHeader_t));
		// parse a single frame
		Status = FrameParser_AudioEAc3_c::ParseSingleFrameHeader( NextFrameHeader, &NextParsedFrameHeader, false );
		
		if (Status !=  FrameParserNoError) 
		{
			return (Status);
		}
		
		if ((NextParsedFrameHeader.Type == TypeEac3Ind) || (NextParsedFrameHeader.Type == TypeAc3))
		{
			if (NumberOfIndependantSubStreams == 0)
			{
				/* get the first independant stream properties */
				memcpy(ParsedFrameHeader, &NextParsedFrameHeader, sizeof(EAc3AudioParsedFrameHeader_t));
			}
			NumberOfIndependantSubStreams++;
			FrameSize += NextParsedFrameHeader.Length;
			NumberOfSamples += NextParsedFrameHeader.NumberOfSamples;
		}
		else if (NextParsedFrameHeader.Type == TypeEac3Dep)
		{
			if (NumberOfIndependantSubStreams == 0)
			{
				FRAME_ERROR("Dependant subframe found before independant one, should not occur...\n");
				/* we met a dependant substream first, this is a frame parsing error ....*/
				return FrameParserError;
			}
			
			FrameSize += NextParsedFrameHeader.Length;
			NumberOfDependantSubStreams++;
		}
		else
		{
			// what else could it be? raise an error!
			FRAME_ERROR("Bad substream type: %d...\n", NextParsedFrameHeader.Type);
			return FrameParserError;
		}
		
      StreamIndex += NextParsedFrameHeader.Length;
      
    } while (StreamIndex < GivenFrameSize);
  
	if (FrameSize != GivenFrameSize)
    {
		FRAME_ERROR("Given frame size mismatch: %d (expected:%d)\n", FrameSize, GivenFrameSize);
		return FrameParserError;
    }

	if (NumberOfSamples != EAC3_NBSAMPLES_NEEDED)
	{
		FRAME_ERROR("Number of samples mismatch: %d (expected:%d)\n", NumberOfSamples, EAC3_BYTES_NEEDED);
		return FrameParserError;		
	}
	
	// remember the last frame header type to know if this stream is DD+ or Ac3 (dependant eac3 substreams 
	// are always located after eac3 independant substreams or ac3 substreams)
	ParsedFrameHeader->Type    = NextParsedFrameHeader.Type;
	ParsedFrameHeader->Length  =  FrameSize;
	ParsedFrameHeader->NumberOfSamples = NumberOfSamples;

	FRAME_DEBUG("SamplingFrequency %d, FrameSize %d, Indp substreams: %d, Dep substreams: %d\n",
				ParsedFrameHeader->SamplingFrequency, 
				ParsedFrameHeader->Length, 
				NumberOfIndependantSubStreams, 
				NumberOfDependantSubStreams);

    if (FirstTime)
    {
        if (ParsedFrameHeader->Type == TypeAc3)
        {
            FRAME_TRACE("AC3 stream properties: SamplingFrequency %d, FrameSize %d\n",
                        ParsedFrameHeader->SamplingFrequency, 
                        ParsedFrameHeader->Length);
        }
        else
        {
            FRAME_TRACE("DD+ stream properties: SamplingFrequency %d, FrameSize %d, Indp substreams: %d, Dep substreams: %d\n",
                        ParsedFrameHeader->SamplingFrequency, 
                        ParsedFrameHeader->Length, 
                        NumberOfIndependantSubStreams, 
                        NumberOfDependantSubStreams);
        }

        FirstTime = false;
    }
        
	
	return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Examine the supplied frame header and extract the information contained
/// within.
///
/// This is a utility function shared by the frame parser and the equivalent
/// collator. Due to its shared nature this is a static method and does not
/// access any information not provided via the function arguments.
///
/// <b>AC3 Bit stream</b>
///
/// From RFC-4184 (http://www.rfc-editor.org/rfc/rfc4184.txt).
///
/// <pre>
/// AC-3 bit streams are organized into synchronization frames.  Each AC-3 frame
/// contains a Sync Information (SI) field, a Bit Stream Information (BSI) field,
/// and 6 audio blocks (AB), each representing 256 PCM samples for each channel.
/// The entire frame represents a time duration of 1536 PCM samples across all
/// coded channels (e.g., 32 msec @ 48kHz sample rate). Figure 1 shows the AC-3
/// frame format.
/// 
/// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
/// |SI |BSI|  AB0  |  AB1  |  AB2  |  AB3  |  AB4  |  AB5  |AUX|CRC|
/// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
/// 
///                     Figure 1. AC-3 Frame Format
/// 
/// The Synchronization Information field contains information needed to acquire
/// and maintain codec synchronization.  The Bit Stream Information field contains
/// parameters that describe the coded audio service.  Each audio block also
/// contains fields that indicate the use of various coding tools: block switching,
/// dither, coupling, and exponent strategy.  They also contain metadata,
/// optionally used to enhance the playback, such as dynamic range control. Figure
/// 2 shows the structure of an AC-3 audio block.  Note that field sizes vary
/// depending on the coded data.
/// 
/// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
/// |  Block  |Dither |Dynamic    |Coupling |Coupling     |Exponent |
/// |  switch |Flags  |Range Ctrl |Strategy |Coordinates  |Strategy |
/// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
/// |     Exponents       | Bit Allocation  |        Mantissas      |
/// |                     | Parameters      |                       |
/// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
/// 
///                   Figure 2. AC-3 Audio Block Format
///
/// </pre>
///
/// E-AC3 Bit stream decoding is done with the BD following assumptions:
/// Independant and dependant E-AC3 substreams are always tagged as belonging to programme 0 
/// (so no need to filter programmes in e-ac3 bitstream)
///
/// \return Frame parser status code, FrameParserNoError indicates success.
///
FrameParserStatus_t FrameParser_AudioEAc3_c::ParseSingleFrameHeader( unsigned char *FrameHeaderBytes, 
																	 EAc3AudioParsedFrameHeader_t *ParsedFrameHeader,
																	 bool SearchForConvSync)
{
  unsigned int    SamplingFrequency;
  unsigned int    FrameSize;
  unsigned int    NbSamples;
  unsigned char   Bsid;
  Ac3StreamType_t Type;
  bool            convsync = false;
  BitStreamClass_c	Bits;

  Bits.SetPointer(FrameHeaderBytes);

  unsigned short SyncWord  = Bits.Get(16);
  
  if (SyncWord != EAC3_START_CODE)
  {
	  if (SearchForConvSync == false)
	  {
		  FRAME_ERROR("Invalid start code 0x%04x\n", SyncWord);
	  }
	  return FrameParserError;
  }

  Bits.SetPointer(&FrameHeaderBytes[5]);

  Bsid = Bits.Get(5);

  if ( Bsid <= 8 )                              // bsid <= 8 and bsid >= 0
  {
	  unsigned int   RateCode;
	  unsigned int   FsCode;
	  unsigned int   BitRate;

      Type = TypeAc3;

	  Bits.SetPointer(&FrameHeaderBytes[4]);
      
	  FsCode  = Bits.Get(2);
          if (FsCode >= 3)
	  {
	      FRAME_ERROR("Invalid frequency code\n");
	      return FrameParserError;
	  }
		  
      RateCode = Bits.Get(6);
	  
      if (RateCode >= 38)
	  {
		  FRAME_ERROR("Invalid rate code\n");
		  return FrameParserError;
	  }
      
      BitRate = AC3Rate[RateCode >> 1];
      
	  SamplingFrequency = EAC3SamplingFreq[FsCode];
	  FrameSize = 2 * ((FsCode == 1) ? ((320 * BitRate / 147 + (RateCode & 1))): (EAC3FsCodeToFrameSize[FsCode] * BitRate));
      NbSamples =  1536;
  }
  else if ((Bsid > 10) || (Bsid <= 16))                              // bsid >10 and bsid <= 16
  {			
	  Bits.SetPointer(&FrameHeaderBytes[2]);
      char StrmType = Bits.Get(2);
	  int fscod;
	  int fscod2_numblk;
	  
      /* sub stream id (used for programme identification) */
      ParsedFrameHeader->SubStreamId =  Bits.Get(3);
      
      if ((StrmType == 0) || (StrmType == 2))
	  {
		  /* independant stream or substream */
		  Type =  TypeEac3Ind;
	  }
	  else
	  {
		  /* dependant stream or substream */
		  Type =  TypeEac3Dep;
	  }
      
      FrameSize = (Bits.Get(11) + 1) * 2;

	  fscod = Bits.Get(2);
	  fscod2_numblk = Bits.Get(2);

	  if (fscod == 3)
	  {
		  SamplingFrequency = EAC3SamplingFreq[fscod2_numblk] >> 1;
		  fscod2_numblk = 3; /* six blocks per frame */
	  }
	  else
	  {
		  SamplingFrequency =  EAC3SamplingFreq[fscod];

		  if ((fscod2_numblk != 3) && (SearchForConvSync))
		  {
			  // search for a convsync to be sure we are at the first block 
			  // out of six (if we transcode to spdif into ac3)...
			  int acmod = Bits.Get(3);
			  int lfeon = Bits.Get(1);
			  int bsid = Bits.Get(5);
			  int dialnorm = Bits.Get(5);

			  FRAME_DEBUG("StrmType: %d, substream id: %d, FrameSize: %d, fscod: %d, fscod2_numblk: %d\n",
						  StrmType, ParsedFrameHeader->SubStreamId, FrameSize, fscod, fscod2_numblk);

			  FRAME_DEBUG("acmod: %d, lfeon: %d, bsid: %d, dialnorm: %d\n",
						  acmod, lfeon, bsid, dialnorm);

			  if (Bits.Get(1))  // compre
			  {
				  FRAME_DEBUG("compre on\n");
				  Bits.FlushUnseen(8); // compr
			  }

			  if (acmod == 0) /* if 1+1 mode */
			  {
				  Bits.FlushUnseen(5); // dialnorm2
				  if (Bits.Get(1)) //compr2e
					  Bits.FlushUnseen(8); // compr
			  }

			  if (StrmType == 1) /* if dependent stream */
			  {
				  if (Bits.Get(1)) //chanmape
					  Bits.FlushUnseen(16); // chanmap
			  }
			  if (Bits.Get(1))  // mixmdate
			  {
				  FRAME_DEBUG("mixmdat enabled\n");

				  if (acmod > 2) /* if more than 2 channels */
				  {
					  Bits.FlushUnseen(2); // dmixmod
				  }
				  if ((acmod & 1) && (acmod > 2)) /* if three front channels exists */
				  {
					  Bits.FlushUnseen(6); // l[ot]rtcmixlvl
				  }
				  if (acmod & 4) /* if a surround channel exists */
				  {
					  Bits.FlushUnseen(6); // l[ot]r[ot]surmixlvl
				  }
				  if (lfeon) /* if the LFE channel exists */
				  {
					  if (Bits.Get(1)) // lfemixlevcode
					  {
						  Bits.FlushUnseen(5); // lfemixlevcod
					  }
				  }
				  if (StrmType == 0) /* if independent stream */
				  {
					  if (Bits.Get(1)) //pgmscle
						  Bits.FlushUnseen(6); // pgmscl

					  if (acmod == 0) /* if 1+1 mode */
					  {
						  if (Bits.Get(1)) //pgmscl2e
							  Bits.FlushUnseen(6); // pgmscl2
					  }

					  if (Bits.Get(1)) //extpgmscle
						  Bits.FlushUnseen(6); // extpgmscl
					  
					  unsigned char mixdef = Bits.Get(2);
					  if (mixdef == 1)
					  {
						  Bits.FlushUnseen(5); // premixcompsel, drcsrc, premixcompscl
					  }
					  else if (mixdef == 2)
					  {
						  Bits.FlushUnseen(12); // mixdata
					  }
					  else if (mixdef == 3)
					  {
						  unsigned int mixdeflen = Bits.Get(5);
						  Bits.FlushUnseen(8*(mixdeflen+2)); // mixdata
					  }
					  if (acmod < 2) /* if mono or dual mono source */
					  {
						  if (Bits.Get(1)) //paninfoe
							  Bits.FlushUnseen(14); // panmean, paninfo

						  if (acmod == 0) /* if 1+1 mode */
						  {
							  if (Bits.Get(1)) //paninfo2e
								  Bits.FlushUnseen(14); // panmean2, paninfo2
						  }
					  }
				  }
				  if (Bits.Get(1)) // frmmixcfginfoe
				  {
					  if (fscod2_numblk == 0)
					  {
						  Bits.FlushUnseen(5); //blkmixcfginfo[0]
					  }
					  else
					  {
						  int blk;
						  
						  for (blk = 0; blk < EAC3Blocks[fscod2_numblk]; blk++)
						  {
							  if (Bits.Get(1)) //blkmixcfginfoe
								  Bits.FlushUnseen(5); //blkmixcfginfo[blk]
						  }
					  }
				  }
			  }
			  if (Bits.Get(1))  // infomdate
			  {
				  FRAME_DEBUG("infomdat enabled\n");

				  Bits.FlushUnseen(5); //bsmod, copyrightb, origbs
				  if (acmod == 2) /* if in 2/0 mode */
					  Bits.FlushUnseen(4); //dsurmod, dheadphonmod

				  if (acmod >= 6) /* if both surround channels exists */					  
					  Bits.FlushUnseen(2); //dsurexmod

				  if (Bits.Get(1))  // audprodie
					  Bits.FlushUnseen(8); // mixlevel, roomtyp, adconvtyp
				  
				  if (acmod == 0) /* if 1+1 mode */
				  {
					  if (Bits.Get(1))  // audprodi2e
						  Bits.FlushUnseen(8); // mixlevel2, roomtyp2, adconvtyp2
				  }
				  if (fscod < 3) /* if not half sample rate */
					  Bits.FlushUnseen(1); // sourcefscod
			  }

			  if ((StrmType == 0) && (fscod2_numblk != 3))
			  {
				  // we finally got it!!!!!!!
				  convsync = Bits.Get(1);
				  FRAME_DEBUG("Reached convsync: value = %d\n", convsync);

			  }
		  }
	  }
	  
	  NbSamples =  256 * EAC3Blocks[fscod2_numblk];
  }
  else
  {
	  FRAME_ERROR("Invalid BSID\n");
	  return FrameParserError;
  }
  
  FRAME_DEBUG("SamplingFrequency: %d, FrameSize: %d, Frame type: % d, NbSamples: %d, convsync: %d \n",
			  SamplingFrequency, FrameSize, Type, NbSamples, convsync);
  
  //
  
  ParsedFrameHeader->Type = Type;
  ParsedFrameHeader->SamplingFrequency = SamplingFrequency;
  ParsedFrameHeader->NumberOfSamples = NbSamples;
  ParsedFrameHeader->Length = FrameSize; 
  ParsedFrameHeader->FirstBlockForTranscoding = convsync;
  //
  return FrameParserNoError;
}


///////////////////////////////////////////////////////////////////////////
///
///     Constructor
///
FrameParser_AudioEAc3_c::FrameParser_AudioEAc3_c( void )
{
    Configuration.FrameParserName		= "AudioEAc3";

    Configuration.StreamParametersCount		= 32;
    Configuration.StreamParametersDescriptor	= &EAc3AudioStreamParametersBuffer;

    Configuration.FrameParametersCount		= 32;
    Configuration.FrameParametersDescriptor	= &EAc3AudioFrameParametersBuffer;

//

    Reset();
}

////////////////////////////////////////////////////////////////////////////
///
///     Destructor
///
FrameParser_AudioEAc3_c::~FrameParser_AudioEAc3_c( void )
{
    Halt();
    Reset();
}

////////////////////////////////////////////////////////////////////////////
///
///     The Reset function release any resources, and reset all variable
///
FrameParserStatus_t   FrameParser_AudioEAc3_c::Reset(  void )
{
    memset( &CurrentStreamParameters, 0, sizeof(CurrentStreamParameters) );

    FirstTime = true;

    return FrameParser_Audio_c::Reset();
}


////////////////////////////////////////////////////////////////////////////
///
///     The register output ring function
///
FrameParserStatus_t   FrameParser_AudioEAc3_c::RegisterOutputBufferRing(       Ring_t          Ring )
{
    //
    // Clear our parameter pointers
    //

    StreamParameters                    = NULL;
    FrameParameters                     = NULL;

    //
    // Pass the call down the line
    //

    return FrameParser_Audio_c::RegisterOutputBufferRing( Ring );
}


////////////////////////////////////////////////////////////////////////////
///
/// Parse the frame header and store the results for when we emit the frame.
///
FrameParserStatus_t   FrameParser_AudioEAc3_c::ReadHeaders( void )
{
	FrameParserStatus_t Status;
	
    //
    // Perform the common portion of the read headers function
    //

    FrameParser_Audio_c::ReadHeaders();

    //
	
	Status = ParseFrameHeader( BufferData, &ParsedFrameHeader, BufferLength );
    if( Status != FrameParserNoError )
    {
    	FRAME_ERROR("Failed to parse frame header, bad collator selected?\n");
    	return Status;
    }
    
    if (ParsedFrameHeader.Length != BufferLength)
    {
    	FRAME_ERROR("Buffer length (%d) is inconsistant with frame header (%d), bad collator selected?\n",
    	            BufferLength, ParsedFrameHeader.Length);
    	return FrameParserError;
    }

    FrameToDecode = true;
 
    Status = GetNewFrameParameters( (void **) &FrameParameters );
    if( Status != FrameParserNoError )
    {
    	FRAME_ERROR( "Cannot get new frame parameters\n" );
    	return Status;
    }

    // Nick inserted some default values here
    ParsedFrameParameters->FirstParsedParametersForOutputFrame          = true;
    ParsedFrameParameters->FirstParsedParametersAfterInputJump          = FirstDecodeAfterInputJump;
    ParsedFrameParameters->SurplusDataInjected                          = SurplusDataInjected;
    ParsedFrameParameters->ContinuousReverseJump                        = ContinuousReverseJump;
    ParsedFrameParameters->KeyFrame                                     = true;
    ParsedFrameParameters->ReferenceFrame                               = false;

    ParsedFrameParameters->NewFrameParameters		 = true;
    ParsedFrameParameters->SizeofFrameParameterStructure = sizeof(EAc3AudioFrameParameters_t);
    ParsedFrameParameters->FrameParameterStructure       = FrameParameters;
    
    FrameParameters->FrameSize = ParsedFrameHeader.Length;

    ParsedAudioParameters->Source.BitsPerSample = 0; // filled in by codec
    ParsedAudioParameters->Source.ChannelCount = 0;  // filled in by codec
    ParsedAudioParameters->Source.SampleRateHz = ParsedFrameHeader.SamplingFrequency;
    ParsedAudioParameters->SampleCount = ParsedFrameHeader.NumberOfSamples;
    ParsedAudioParameters->Organisation = 0; // filled in by codec
    ParsedAudioParameters->OriginalEncoding = (ParsedFrameHeader.Type == TypeAc3)?AudioOriginalEncodingAc3:AudioOriginalEncodingDdplus;

    return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
///     The reset reference frame list function
///
FrameParserStatus_t   FrameParser_AudioEAc3_c::ResetReferenceFrameList( void )
{
    FRAME_DEBUG(">><<");
    Player->CallInSequence( Stream, SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, CODEC_RELEASE_ALL );

    return FrameParserNoError;
}


///////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for EAC3 audio.
///
/// \copydoc FrameParser_Audio_c::PurgeQueuedPostDecodeParameterSettings()
///
FrameParserStatus_t   FrameParser_AudioEAc3_c::PurgeQueuedPostDecodeParameterSettings( void )
{
    return FrameParserNoError;
}


///////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for EAC3 audio.
///
/// \copydoc FrameParser_Audio_c::ProcessQueuedPostDecodeParameterSettings()
///
FrameParserStatus_t   FrameParser_AudioEAc3_c::ProcessQueuedPostDecodeParameterSettings( void )
{
    return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Determine the display frame index and presentation time of the decoded frame.
///
/// For EAC3 audio these can be determined immediately (although it the first
/// frame for decode does not contain a PTS we must synthesize one).
///
FrameParserStatus_t   FrameParser_AudioEAc3_c::GeneratePostDecodeParameterSettings( void )
{
FrameParserStatus_t Status;

//
    
    //
    // Default setting
    //

    ParsedFrameParameters->DisplayFrameIndex            = INVALID_INDEX;
    ParsedFrameParameters->NativePlaybackTime           = INVALID_TIME;
    ParsedFrameParameters->NormalizedPlaybackTime       = INVALID_TIME;
    ParsedFrameParameters->NativeDecodeTime             = INVALID_TIME;
    ParsedFrameParameters->NormalizedDecodeTime         = INVALID_TIME;

    //
    // Record in the structure the decode and presentation times if specified
    //

    if( CodedFrameParameters->PlaybackTimeValid )
    {
	ParsedFrameParameters->NativePlaybackTime       = CodedFrameParameters->PlaybackTime;
	TranslatePlaybackTimeNativeToNormalized( CodedFrameParameters->PlaybackTime, &ParsedFrameParameters->NormalizedPlaybackTime );
    }

    if( CodedFrameParameters->DecodeTimeValid )
    {
	ParsedFrameParameters->NativeDecodeTime         = CodedFrameParameters->DecodeTime;
	TranslatePlaybackTimeNativeToNormalized( CodedFrameParameters->DecodeTime, &ParsedFrameParameters->NormalizedDecodeTime );
    }

    //
    // Sythesize the presentation time if required
    //
    
    Status = HandleCurrentFrameNormalizedPlaybackTime();
    if( Status != FrameParserNoError )
    {
    	return Status;
    }
    
    //
    // We can't fail after this point so this is a good time to provide a display frame index
    //
    
    ParsedFrameParameters->DisplayFrameIndex		 = NextDisplayFrameIndex++;
    
    //
    // Use the super-class utilities to complete our housekeeping chores
    //
    
    // no call to HandleUpdateStreamParameters() because UpdateStreamParameters is always false
    FRAME_ASSERT( false == UpdateStreamParameters && NULL == StreamParametersBuffer );

    GenerateNextFrameNormalizedPlaybackTime(ParsedFrameHeader.NumberOfSamples,
                                            ParsedFrameHeader.SamplingFrequency);

//
    //DumpParsedFrameParameters( ParsedFrameParameters, __PRETTY_FUNCTION__ );
    return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for EAC3 audio.
///
FrameParserStatus_t   FrameParser_AudioEAc3_c::PrepareReferenceFrameList( void )
{
    return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for EAC3 audio.
///
FrameParserStatus_t   FrameParser_AudioEAc3_c::UpdateReferenceFrameList( void )
{
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for EAC3 audio.
///
FrameParserStatus_t   FrameParser_AudioEAc3_c::ProcessReverseDecodeUnsatisfiedReferenceStack( void )
{
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for EAC3 audio.
///
FrameParserStatus_t   FrameParser_AudioEAc3_c::ProcessReverseDecodeStack(			void )
{
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for EAC3 audio.
///
FrameParserStatus_t   FrameParser_AudioEAc3_c::PurgeReverseDecodeUnsatisfiedReferenceStack(	void )
{
	
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for EAC3 audio.
///
FrameParserStatus_t   FrameParser_AudioEAc3_c::PurgeReverseDecodeStack(			void )
{
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for EAC3 audio.
///
FrameParserStatus_t   FrameParser_AudioEAc3_c::TestForTrickModeFrameDrop(			void )
{
    return FrameParserNoError;
}


