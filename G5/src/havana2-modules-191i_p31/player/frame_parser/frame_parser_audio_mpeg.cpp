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

Source file name : frame_parser_audio_mpeg.cpp
Author :           Daniel

Implementation of the mpeg audio frame parser class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
30-Mar-07   Created (from frame_parser_video_mpeg2.cpp)     Daniel

************************************************************************/

////////////////////////////////////////////////////////////////////////
///
/// \class FrameParser_AudioMpeg_c
/// \brief Frame parser for MPEG audio (layer I, II and III).
///

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "mpeg_audio.h"
#include "frame_parser_audio_mpeg.h"

//


// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

#undef FRAME_TAG
#define FRAME_TAG "MPEG audio frame parser"

static BufferDataDescriptor_t     MpegAudioStreamParametersBuffer = BUFFER_MPEG_AUDIO_STREAM_PARAMETERS_TYPE;
static BufferDataDescriptor_t     MpegAudioFrameParametersBuffer = BUFFER_MPEG_AUDIO_FRAME_PARAMETERS_TYPE;

//
// Bit rate lookup table for MPEG audio frame parsing
//

static unsigned int Mpeg1BitRateLookup[3][16] =
{
    { 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0 },  /* Layer I */
    { 0, 32, 48, 56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 384, 0 },  /* Layer II */
    { 0, 32, 40, 48,  56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 0 }   /* Layer III */
};

static unsigned int Mpeg2BitRateLookup[3][16] =
{
    { 0, 32, 48, 56,  64,  80,  96, 112, 128, 144, 160, 176, 192, 224, 256, 0 },  /* Layer I */
    { 0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160, 0 },  /* Layer II */
    { 0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160, 0 }   /* Layer III */
};

//
// Sample rate lookup table for MPEG audio frame parsing
//

static unsigned int SamplingFrequencyLookup[3][4] =
{
    { 44100, 48000, 32000, 0 }, /* MPEG-1 */
    { 22050, 24000, 16000, 0 }, /* MPEG-2 */
    { 11025, 12000,  8000, 0 }  /* MPEG-2.5 */
    //    { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 0, 0, 0};
};

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined macros
//


// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

////////////////////////////////////////////////////////////////////////////
///
/// Examine the supplied frame header and extract the information contained within.
///
/// This is a utility function shared by the frame parser and the equivalent
/// collator. Due to its shared nature this is a static method and does not access
/// any information not provided via the function arguments.
///
/// <b>MPEG format</b>
///
/// <pre>
/// AAAAAAAA AAABBCCD EEEEFFGH IIJJKLMM
///
/// Sign            Length          Position         Description
/// 
/// A                11             (31-21)          Frame sync (all bits set)
/// B                 2             (20,19)          MPEG Audio version ID
/// 						     00 - MPEG Version 2.5 (unofficial) (looks like actually this is mpeg4)
/// 						     01 - reserved (or this is)
/// 						     10 - MPEG Version 2 (ISO/IEC 13818-3)
/// 						     11 - MPEG Version 1 (ISO/IEC 11172-3)
/// C                 2             (18,17)          Layer description
/// 						     00 - reserved
/// 						     01 - Layer III
/// 						     10 - Layer II
/// 						     11 - Layer I
/// D                 1             (16)             Protection bit
/// 						     0 - Protected by CRC (16bit crc follows header)
/// 						     1 - Not protected
/// E                 4             (15,12)          Bitrate index
/// 						     bits V1,L1 V1,L2 V1,L3 V2,L1 V2, L2 & L3
/// 						     0000 free  free  free  free  free
/// 						     0001 32    32    32    32    8
/// 						     0010 64    48    40    48    16
/// 						     0011 96    56    48    56    24
/// 						     0100 128   64    56    64    32
/// 						     0101 160   80    64    80    40
/// 						     0110 192   96    80    96    48
/// 						     0111 224   112   96    112   56
/// 						     1000 256   128   112   128   64
/// 						     1001 288   160   128   144   80
/// 						     1010 320   192   160   160   96
/// 						     1011 352   224   192   176   112
/// 						     1100 384   256   224   192   128
/// 						     1101 416   320   256   224   144
/// 						     1110 448   384   320   256   160
/// 						     1111 bad   bad   bad   bad   bad
/// 
/// F                2              (11,10)          Sampling rate frequency index (values are in Hz)
/// 						     bits   MPEG1   MPEG2   MPEG2.5
/// 						     00     44100   22050   11025
/// 						     01     48000   24000   12000
/// 						     10     32000   16000   8000
/// 						     11   reserv. reserv. reserv.
/// G                1              (9)              Padding bit
/// 						     0 - frame is not padded
/// 						     1 - frame is padded with one extra slot
/// 						     Padding is used to fit the bit rates exactly.
/// 						     Eg: 128k 44.1kHz layer II uses a lot of 418 bytes and some of 417 bytes
/// 						     long frames to get the exact 128k bitrate.
/// 						     For Layer I slot is 32 bits long, for Layer II and Layer III
/// 						     slot is 8 bits long.
/// H                1              (8)             Private bit. It may be freely used for specific needs of an application.
/// I                2              (7,6)           Channel mode
/// 						     00 - Stereo
/// 						     01 - Joint stereo (Stereo)
/// 						     10 - Dual channel (2 mono channels)
/// 						     11 - Single channel (Mono)
/// </pre>
/// 
/// \return Frame parser status code, FrameParserNoError indicates success.
///
FrameParserStatus_t FrameParser_AudioMpeg_c::ParseFrameHeader( unsigned char *FrameHeaderBytes, MpegAudioParsedFrameHeader_t *ParsedFrameHeader )
{
unsigned int    FrameHeader;
int             LayerIndex;
int             MpegIndex;
unsigned int    BitRateIndex;
unsigned int    BitRate;
unsigned int    SamplingFrequencyIndex;
unsigned int    SamplingFrequency;
unsigned int    Padding;
unsigned int    FrameSlots;

// these are former members of the Player1 MPEG frame analyser class
unsigned int    FrameSize;
unsigned int    NumSamplesPerChannel;

//

    FrameHeader = FrameHeaderBytes[0] << 24 | FrameHeaderBytes[1] << 16 |
                  FrameHeaderBytes[2] <<  8 | FrameHeaderBytes[3];

    if ( (FrameHeader & MPEG_AUDIO_START_CODE_MASK) != MPEG_AUDIO_START_CODE )
    {
    	FRAME_ERROR( "Invalid start code %x\n", FrameHeader & MPEG_AUDIO_START_CODE_MASK );
    	return FrameParserError;
    }
    
    switch( (FrameHeader & MPEG_AUDIO_LAYER_MASK) )
    {
	case MPEG_AUDIO_LAYER_I:   LayerIndex = 0;
			break;

	case MPEG_AUDIO_LAYER_II:  LayerIndex = 1;
			break;

	case MPEG_AUDIO_LAYER_III: LayerIndex = 2;
			break;

	default:        FRAME_ERROR( "Invalid layer %d\n",  FrameHeader & MPEG_AUDIO_LAYER_MASK );
			return FrameParserError;
			break;
    }

    switch( (FrameHeader & MPEG_AUDIO_MPEG_MASK) )
    {
	case MPEG_AUDIO_MPEG_1:    MpegIndex = 0;
			break;

	case MPEG_AUDIO_MPEG_2:    MpegIndex = 1;
			break;

	case MPEG_AUDIO_MPEG_2_5:  MpegIndex = 2;
			break;

	default:        FRAME_ERROR( "Bad MPEG selection\n" );
			return FrameParserError;
			break;
    }

    BitRateIndex                = (FrameHeader & MPEG_AUDIO_BIT_RATE_MASK) >> MPEG_AUDIO_BIT_RATE_SHIFT;
    SamplingFrequencyIndex      = (FrameHeader & MPEG_AUDIO_SAMPLING_FREQUENCY_MASK) >> MPEG_AUDIO_SAMPLING_FREQUENCY_SHIFT;
    Padding                     = (FrameHeader & MPEG_AUDIO_PADDING_MASK) >> MPEG_AUDIO_PADDING_SHIFT;

    BitRate                     = (MpegIndex == 0) ? Mpeg1BitRateLookup[LayerIndex][BitRateIndex] :
						     Mpeg2BitRateLookup[LayerIndex][BitRateIndex];
    if( BitRate == 0 )
    {
	FRAME_ERROR( "Invalid bit rate, %x, %x, %x\n",
				FrameHeader, MpegIndex, LayerIndex, BitRateIndex);
	return FrameParserError;
    }

    SamplingFrequency           = SamplingFrequencyLookup[MpegIndex][SamplingFrequencyIndex];
    if( SamplingFrequency == 0 )
    {
	FRAME_ERROR( "Invalid frequency, %x, %x\n",
				FrameHeader, MpegIndex, SamplingFrequencyIndex);
	return FrameParserError;
    }

    //
    //    Duration calculated as :-
    //      Layer I = 384 samples per frame
    //      Layer II or III = 1152 samples per frame
    //
    //      e.g. Layer II @ 48kHz, is (1152 / 48000) seconds (times 1000000 to convert to microseconds)
    //
    //    FrameSize calculated as ISO 11172-3 2.4.3.1


    if( LayerIndex == 0 )
    {
	/* Layer I */
	FrameSlots              = ((12000 * BitRate) / SamplingFrequency) + Padding;
	FrameSize               = 4 * FrameSlots;
	NumSamplesPerChannel    = 384;
    }
    else if ( (LayerIndex == 2) && (MpegIndex == 1 || MpegIndex == 2) )
    {
	/* Special case for MPEG2-LSF Layer III */
	FrameSize               = ((72000 * BitRate) / SamplingFrequency) + Padding;
	NumSamplesPerChannel    = 576;
    }
    else
    {
	/* Layer II or Layer III without LSF */
	FrameSize               = ((144000 * BitRate) / SamplingFrequency) + Padding;
	NumSamplesPerChannel    = 1152;
    }

    //

    FRAME_DEBUG( "Header %8x, MPEG%dL%d, BitRate %3d, Frequency %5d, FrameSize %d, Padding %d\n",
                 FrameHeader, MpegIndex+1, LayerIndex+1, BitRate, SamplingFrequency, FrameSize, Padding );

    //
    
    ParsedFrameHeader->Header = FrameHeader;

    ParsedFrameHeader->Layer = LayerIndex + 1;
    ParsedFrameHeader->MpegStandard = (MpegIndex == 2 ? 25 : MpegIndex + 1);
    ParsedFrameHeader->BitRate = BitRate;
    ParsedFrameHeader->SamplingFrequency = SamplingFrequency;
    ParsedFrameHeader->PaddedFrame = Padding;
    
    ParsedFrameHeader->NumberOfSamples = NumSamplesPerChannel;
    ParsedFrameHeader->Length = FrameSize;
    
    return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Examine the supplied extension header and determine the length of the extension.
///
/// Extension headers are described in ISO/IEC 13818-3.
///
/// This is a utility function shared by the frame parser and the equivalent
/// collator. Due to its shared nature this is a static method and does not access
/// any information not provided via the function arguments.
///
/// <b>Header format</b>
///
/// <pre>
/// AAAAAAAA AAAABBBB BBBBBBBB BBBBCCCC CCCCCCCD (40 bits)
///
/// Sign            Length          Description
/// 
/// A                12             Frame sync (0x7ff)
/// B                16             CRC
/// C                11             Length in bytes
/// D                1              ID (reserved - set to zero)
/// </pre>
///
/// \return Frame parser status code, FrameParserNoError indicates success.
///
FrameParserStatus_t FrameParser_AudioMpeg_c::ParseExtensionHeader(unsigned char *ExtensionHeader, unsigned int *ExtensionLength)
{
    BitStreamClass_c       Bits;
    
    Bits.SetPointer( ExtensionHeader );

    unsigned int frameSync = Bits.Get( 12 );
    
    if( 0x7ff != frameSync )
    {
	// not an printk error because this method is called speculatively
	FRAME_DEBUG( "Invalid start code %x\n", frameSync );
	return FrameParserError;
    }
    
    unsigned int crc = Bits.Get(16);
    
    unsigned int lengthInBytes = Bits.Get(11);

    unsigned int id = Bits.Get(1);
    if( 0 != id )
    {
	FRAME_ERROR( "Invalid (reserved) ID %x\n", id );
	return FrameParserError;
    }
    
    FRAME_DEBUG( "FrameSync %03x  CRC %04x  FrameSize %d  ID %d\n", frameSync, crc, lengthInBytes, id );
    
    *ExtensionLength = lengthInBytes;
    return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
///     Constructor
///
FrameParser_AudioMpeg_c::FrameParser_AudioMpeg_c( void )
{
    Configuration.FrameParserName		= "AudioMpeg";

    Configuration.StreamParametersCount		= 32;
    Configuration.StreamParametersDescriptor	= &MpegAudioStreamParametersBuffer;

    Configuration.FrameParametersCount		= 32;
    Configuration.FrameParametersDescriptor	= &MpegAudioFrameParametersBuffer;

//

    Reset();
}

////////////////////////////////////////////////////////////////////////////
///
///     Destructor
///
FrameParser_AudioMpeg_c::~FrameParser_AudioMpeg_c( void )
{
    Halt();
    Reset();
}

////////////////////////////////////////////////////////////////////////////
///
///     The Reset function release any resources, and reset all variable
///
FrameParserStatus_t   FrameParser_AudioMpeg_c::Reset(  void )
{
    // CurrentStreamParameters is initialized in RegisterOutputBufferRing()

    return FrameParser_Audio_c::Reset();
}


////////////////////////////////////////////////////////////////////////////
///
///     The register output ring function
///
FrameParserStatus_t   FrameParser_AudioMpeg_c::RegisterOutputBufferRing(       Ring_t          Ring )
{
    //
    // Clear our parameter pointers
    //

    StreamParameters                    = NULL;
    FrameParameters                     = NULL;

    //
    // Set illegal state forcing a parameter update on the first frame
    //
    
    memset( &CurrentStreamParameters, 0, sizeof(CurrentStreamParameters) );
    CurrentStreamParameters.Layer = 0; // illegal layer... force frames a parameters update
    
    //
    // Pass the call down the line
    //

    return FrameParser_Audio_c::RegisterOutputBufferRing( Ring );
}


////////////////////////////////////////////////////////////////////////////
///
/// Parse the frame header and store the results for when we emit the frame.
///
FrameParserStatus_t   FrameParser_AudioMpeg_c::ReadHeaders( void )
{
FrameParserStatus_t Status;
unsigned int ExtensionLength;

    //
    // Perform the common portion of the read headers function
    //

    FrameParser_Audio_c::ReadHeaders();

//

    Status = ParseFrameHeader( BufferData, &ParsedFrameHeader );
    if( Status != FrameParserNoError )
    {
    	FRAME_ERROR("Failed to parse frame header, bad collator selected?\n");
    	return Status;
    }
    
    if (ParsedFrameHeader.Length != BufferLength)
    {
	// if the length is wrong that might be because we haven't considered any extensions
	Status = ParseExtensionHeader( BufferData + ParsedFrameHeader.Length, &ExtensionLength );
	
	if( ( Status != FrameParserNoError ) ||
            ( ParsedFrameHeader.Length + ExtensionLength != BufferLength ) )
        {
	    FRAME_ERROR("Buffer length is inconsistant with frame header, bad collator selected?\n");
    	    return FrameParserError;
        }
	
	// that was it, adjust the recorded frame length accordingly
	ParsedFrameHeader.Length += ExtensionLength;
    }

    FrameToDecode = true;
 
    if( CurrentStreamParameters.Layer != ParsedFrameHeader.Layer )
    {
    	Status = GetNewStreamParameters( (void **) &StreamParameters );
    	if( Status != FrameParserNoError )
    	{
    	    FRAME_ERROR( "Cannot get new stream parameters\n" );
    	    return Status;
    	}
    	        
        StreamParameters->Layer = CurrentStreamParameters.Layer = ParsedFrameHeader.Layer;
        
        UpdateStreamParameters = true;
    }

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
    ParsedFrameParameters->SizeofFrameParameterStructure = sizeof(MpegAudioFrameParameters_t);
    ParsedFrameParameters->FrameParameterStructure       = FrameParameters;
    
    // this does look a little pointless but I don't want to trash it until we
    // have got a few more audio formats supported (see \todo in mpeg_audio.h).    
    FrameParameters->BitRate = ParsedFrameHeader.BitRate;
    FrameParameters->FrameSize = ParsedFrameHeader.Length;

    ParsedAudioParameters->Source.BitsPerSample = 0; // filled in by codec
    ParsedAudioParameters->Source.ChannelCount = 0;  // filled in by codec
    ParsedAudioParameters->Source.SampleRateHz = ParsedFrameHeader.SamplingFrequency;
    ParsedAudioParameters->SampleCount = ParsedFrameHeader.NumberOfSamples;
    ParsedAudioParameters->Organisation = 0; // filled in by codec
    
    return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
///     The reset reference frame list function
///
FrameParserStatus_t   FrameParser_AudioMpeg_c::ResetReferenceFrameList( void )
{
    FRAME_DEBUG(">><<");
    Player->CallInSequence( Stream, SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, CODEC_RELEASE_ALL );

    return FrameParserNoError;
}


///////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for MPEG audio.
///
/// \copydoc FrameParser_Audio_c::PurgeQueuedPostDecodeParameterSettings()
///
FrameParserStatus_t   FrameParser_AudioMpeg_c::PurgeQueuedPostDecodeParameterSettings( void )
{
    return FrameParserNoError;
}


///////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for MPEG audio.
///
/// \copydoc FrameParser_Audio_c::ProcessQueuedPostDecodeParameterSettings()
///
FrameParserStatus_t   FrameParser_AudioMpeg_c::ProcessQueuedPostDecodeParameterSettings( void )
{
    return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Determine the display frame index and presentation time of the decoded frame.
///
/// For MPEG audio these can be determined immediately (although it the first
/// frame for decode does not contain a PTS we must synthesize one).
///
FrameParserStatus_t   FrameParser_AudioMpeg_c::GeneratePostDecodeParameterSettings( void )
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
    
    HandleUpdateStreamParameters();

    GenerateNextFrameNormalizedPlaybackTime(ParsedFrameHeader.NumberOfSamples,
                                            ParsedFrameHeader.SamplingFrequency);

//
    //DumpParsedFrameParameters( ParsedFrameParameters, __PRETTY_FUNCTION__ );
    return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for MPEG audio.
///
FrameParserStatus_t   FrameParser_AudioMpeg_c::PrepareReferenceFrameList( void )
{
    return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for MPEG audio.
///
FrameParserStatus_t   FrameParser_AudioMpeg_c::UpdateReferenceFrameList( void )
{
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for MPEG audio.
///
FrameParserStatus_t   FrameParser_AudioMpeg_c::ProcessReverseDecodeUnsatisfiedReferenceStack( void )
{
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for MPEG audio.
///
FrameParserStatus_t   FrameParser_AudioMpeg_c::ProcessReverseDecodeStack(			void )
{
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for MPEG audio.
///
FrameParserStatus_t   FrameParser_AudioMpeg_c::PurgeReverseDecodeUnsatisfiedReferenceStack(	void )
{
	
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for MPEG audio.
///
FrameParserStatus_t   FrameParser_AudioMpeg_c::PurgeReverseDecodeStack(			void )
{
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for MPEG audio.
///
FrameParserStatus_t   FrameParser_AudioMpeg_c::TestForTrickModeFrameDrop(			void )
{
    return FrameParserNoError;
}


