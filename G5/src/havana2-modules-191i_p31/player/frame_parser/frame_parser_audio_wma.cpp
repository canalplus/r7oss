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

Source file name : frame_parser_audio_wma.cpp
Author :           Adam

Implementation of the wma audio frame parser class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
11-Sep-07   Created (from frame_parser_video_mpeg2.cpp)     Adam

************************************************************************/

////////////////////////////////////////////////////////////////////////
///
/// \class FrameParser_AudioWma_c
/// \brief Frame parser for WMA audio
///

#define ENABLE_FRAME_DEBUG 0

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "wma_audio.h"
#include "frame_parser_audio_wma.h"
#include "wma_properties.h"

//


// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

#undef FRAME_TAG
#define FRAME_TAG "WMA audio frame parser"

static BufferDataDescriptor_t     WmaAudioStreamParametersBuffer = BUFFER_WMA_AUDIO_STREAM_PARAMETERS_TYPE;
static BufferDataDescriptor_t     WmaAudioFrameParametersBuffer = BUFFER_WMA_AUDIO_FRAME_PARAMETERS_TYPE;




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
/// Examine the supplied stream header and extract the information contained within.
///
/// This is a utility function shared by the frame parser and the equivalent
/// collator. Due to its shared nature this is a static method and does not access
/// any information not provided via the function arguments.
///
/// \return Frame parser status code, FrameParserNoError indicates success.
///
FrameParserStatus_t FrameParser_AudioWma_c::ParseStreamHeader(unsigned char*               FrameHeaderBytes,
							      WmaAudioStreamParameters_t*  StreamParameters,
							      bool                         Verbose)
{
    unsigned int                         FrameDataLength;
    unsigned int*                        FrameDataU32           = (unsigned int*)FrameHeaderBytes;
    unsigned char*                       FrameData              = (unsigned char*)FrameHeaderBytes;
    ASF_StreamPropertiesObject_c         StreamPropertiesObject;
    WMA_WaveFormatEx_c                   WaveFormatEx;
    WMA_TypeSpecificData_c               TypeSpecificData;

    FrameDataLength     = FrameDataU32[4];
    FrameData           = StreamPropertiesObject.decode (FrameData, FrameDataLength);
    if (Verbose)
	StreamPropertiesObject.dump(true);
    if (!FrameData)
    {
        FRAME_ERROR("Failed to decode ASF stream properties object\n");
        return FrameParserError;
    }

    FrameData            = WaveFormatEx.decode         (StreamPropertiesObject.type_specific_data,
                                                        StreamPropertiesObject.type_specific_data_length);
    if (Verbose)
	WaveFormatEx.dump(true);
    if (!FrameData)
    {
        FRAME_ERROR("Failed to decode audio stream's type specific data (WaveFormatEx)\n");
        return FrameParserError;
    }

    FrameData           = TypeSpecificData.decode      (WaveFormatEx.codec_id,
                                                        WaveFormatEx.codec_specific_data,
                                                        WaveFormatEx.codec_specific_data_size);
    if (Verbose)
	TypeSpecificData.dump(true);
    if (!FrameData)
    {
        FRAME_ERROR("Failed to decode audio stream's type specific data (WMA)\n");
        return FrameParserError;
    }

    // calculate the frame length
    StreamParameters->SamplingFrequency                 = WaveFormatEx.samples_per_second;
    if (StreamParameters->SamplingFrequency <= 16000)
        StreamParameters->SamplesPerFrame               = 512;
    else if (StreamParameters->SamplingFrequency <= 22050)
        StreamParameters->SamplesPerFrame               = 1024;
    else if (StreamParameters->SamplingFrequency <= 32000)
    {
        if (WaveFormatEx.codec_id == WMA_VERSION_1)
            StreamParameters->SamplesPerFrame           = 1024;
        else
            StreamParameters->SamplesPerFrame           = 2048;
    }
    else if (StreamParameters->SamplingFrequency <= 48000)
        StreamParameters->SamplesPerFrame               = 2048;
    else if (StreamParameters->SamplingFrequency <= 96000)
        StreamParameters->SamplesPerFrame               = 4096;
    else
        StreamParameters->SamplesPerFrame               = 8192;

    if ((WaveFormatEx.codec_id == WMA_VERSION_9_PRO) || (WaveFormatEx.codec_id == WMA_LOSSLESS))
    {
        // Samples per block can be modified based on encode options
        unsigned int    Mode    = (TypeSpecificData.encode_options & 0x0006);
        if (Mode == 2)                  // Twice the number of samples
            StreamParameters->SamplesPerFrame         <<= 1;
        else if (Mode == 4)             // Half the number of samples
            StreamParameters->SamplesPerFrame         >>= 1;
        else if (Mode == 6)             // One-fourth the number of samples
            StreamParameters->SamplesPerFrame         >>= 2;
    }
    if (Verbose)
	FRAME_TRACE ("SamplesPerFrame = %d\n", StreamParameters->SamplesPerFrame);

    // calculate the average frame length
    // calculate the average frame duration
    unsigned int SamplingPeriod                         = (1000000000 / StreamParameters->SamplingFrequency);
    unsigned int FrameDuration                          = (SamplingPeriod * StreamParameters->SamplesPerFrame) / 1000;
    FRAME_DEBUG("Average frame duration is %dms\n", FrameDuration);

    //if (StreamParameters->LastFrameDuration == 0)
    //       StreamParameters->LastFrameDuration          = StreamParameters->FrameDuration;

    // pass on the settings to the codec
    //CodecWMAAudioStreamParameters_t Params;
    StreamParameters->StreamNumber                      = StreamPropertiesObject.stream_number;
    StreamParameters->FormatTag                         = WaveFormatEx.codec_id;
    StreamParameters->NumberOfChannels                  = WaveFormatEx.number_of_channels;
    StreamParameters->SamplesPerSecond                  = WaveFormatEx.samples_per_second;
    StreamParameters->AverageNumberOfBytesPerSecond     = WaveFormatEx.average_number_of_bytes_per_second;
    StreamParameters->BlockAlignment                    = WaveFormatEx.block_alignment;
    StreamParameters->BitsPerSample                     = WaveFormatEx.bits_per_sample;

    // Set valit bits per sample.  If type Specific data sets the value use that, otherwise set same as bits per sample
    StreamParameters->ValidBitsPerSample                = (TypeSpecificData.valid_bits_per_sample == 0) ?
                                                                    WaveFormatEx.bits_per_sample :
                                                                    TypeSpecificData.valid_bits_per_sample;
    StreamParameters->ChannelMask                       = TypeSpecificData.channel_mask;

    StreamParameters->SamplesPerBlock                   = TypeSpecificData.samples_per_block;
    if (Verbose)
	FRAME_TRACE ("SamplesPerBlock = %d\n", StreamParameters->SamplesPerBlock);
    StreamParameters->EncodeOptions                     = TypeSpecificData.encode_options;
    StreamParameters->SuperBlockAlign                   = TypeSpecificData.super_block_align;

  return FrameParserNoError;
}
//}}}  


////////////////////////////////////////////////////////////////////////////
///
///     Constructor
///
FrameParser_AudioWma_c::FrameParser_AudioWma_c( void )
{
    Configuration.FrameParserName               = "AudioWma";

    Configuration.StreamParametersCount         = 32;
    Configuration.StreamParametersDescriptor    = &WmaAudioStreamParametersBuffer;

    Configuration.FrameParametersCount          = 32;
    Configuration.FrameParametersDescriptor     = &WmaAudioFrameParametersBuffer;

//

    Reset();
}

////////////////////////////////////////////////////////////////////////////
///
///     Destructor
///
FrameParser_AudioWma_c::~FrameParser_AudioWma_c( void )
{
    Halt();
    Reset();
}

////////////////////////////////////////////////////////////////////////////
///
///     The Reset function release any resources, and reset all variable
///
FrameParserStatus_t   FrameParser_AudioWma_c::Reset(  void )
{
    // CurrentStreamParameters is initialized in RegisterOutputBufferRing()
    LastNormalizedDecodeTime = 0; // must *not* be INVALID_TIME

    return FrameParser_Audio_c::Reset();
}


////////////////////////////////////////////////////////////////////////////
///
///     The register output ring function
///
FrameParserStatus_t   FrameParser_AudioWma_c::RegisterOutputBufferRing(       Ring_t          Ring )
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

    //
    // Pass the call down the line
    //

    return FrameParser_Audio_c::RegisterOutputBufferRing( Ring );
}


////////////////////////////////////////////////////////////////////////////
///
/// Parse the frame header and store the results for when we emit the frame.
///
FrameParserStatus_t   FrameParser_AudioWma_c::ReadHeaders( void )
{
    FrameParserStatus_t Status;

    //
    // Perform the common portion of the read headers function
    //

    FrameParser_Audio_c::ReadHeaders();

    //FRAME_DEBUG("READHEADERS: Len-%d \n", BufferLength);

    //Status = ParseFrameHeader( BufferData, &ParsedFrameHeader );
    //if( Status != FrameParserNoError )
    //{
    //    FRAME_ERROR("Failed to parse frame header, bad collator selected?\n");
    //    return Status;
    //}
    //well, either there was no header, or there is and we need to sort out the stream parameters

    if( 0 == memcmp(BufferData, asf_guid_lookup[ASF_GUID_STREAM_PROPERTIES_OBJECT], sizeof(asf_guid_t)))
    {
        Status          = GetNewStreamParameters ((void**)&StreamParameters);
        if (Status != FrameParserNoError)
        {
            FRAME_ERROR( "Cannot get new stream parameters\n" );
            return Status;
        }
        Status = ParseStreamHeader (BufferData, StreamParameters);
        if (Status != FrameParserNoError)
        {
            FRAME_ERROR ("Failed to parse stream parameters\n" );
            return Status;
        }

        memcpy (&CurrentStreamParameters, StreamParameters, sizeof(CurrentStreamParameters));

        UpdateStreamParameters  = true;
        FrameToDecode           = false;        // throw away the stream properties object - the new stream parameters will
                                                // be sent to the codec attached to the next data block

	LastNormalizedDecodeTime = 0; // reset the last observed decode time whenever we get new parameters
    }
    else
    {
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

        ParsedFrameParameters->NewFrameParameters            = true;
        ParsedFrameParameters->SizeofFrameParameterStructure = sizeof(WmaAudioFrameParameters_t);
        ParsedFrameParameters->FrameParameterStructure       = FrameParameters;

        // this does look a little pointless but I don't want to trash it until we
        // have got a few more audio formats supported (see \todo in mpeg_audio.h).
        //FrameParameters->BitRate                             = CurrentStreamParameters.NumberOfSamples;
        //FrameParameters->FrameSize                           = CurrentStreamParameters.Length;

        ParsedAudioParameters->Source.BitsPerSample          = 0; // filled in by codec
        ParsedAudioParameters->Source.ChannelCount           = 0; // filled in by codec

        //will always be 48K, as 96K will be downsampled
        ParsedAudioParameters->Source.SampleRateHz           = CurrentStreamParameters.SamplingFrequency;

        //derive the sample count from the actual buffer size received
        ParsedAudioParameters->SampleCount                  = CurrentStreamParameters.SamplesPerFrame;  //! use value saved from stream properties object

        ParsedAudioParameters->Organisation                 = 0; // filled in by codec
    }

    return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
///     The reset reference frame list function
///
FrameParserStatus_t   FrameParser_AudioWma_c::ResetReferenceFrameList( void )
{
    FRAME_DEBUG(">><<");
    Player->CallInSequence( Stream, SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, CODEC_RELEASE_ALL );

    return FrameParserNoError;
}


///////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for WMA audio.
///
/// \copydoc FrameParser_Audio_c::PurgeQueuedPostDecodeParameterSettings()
///
FrameParserStatus_t   FrameParser_AudioWma_c::PurgeQueuedPostDecodeParameterSettings( void )
{
    return FrameParserNoError;
}


///////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for WMA audio.
///
/// \copydoc FrameParser_Audio_c::ProcessQueuedPostDecodeParameterSettings()
///
FrameParserStatus_t   FrameParser_AudioWma_c::ProcessQueuedPostDecodeParameterSettings( void )
{
    return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Determine the display frame index and presentation time of the decoded frame.
///
/// For WMA audio these can be determined immediately (although it the first
/// frame for decode does not contain a PTS we must synthesize one).
///
FrameParserStatus_t   FrameParser_AudioWma_c::GeneratePostDecodeParameterSettings( void )
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
    else
    {
        FRAME_DEBUG("No timestamp (collator did not provide one)\n");
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
        report( severity_info, "POSTDECODE: HandleCurrentFrameNormalizedPlaybackTime failed \n");
        FRAME_ERROR("POSTDECODE: HandleCurrentFrameNormalizedPlaybackTime failed \n");
        return Status;
    }

    //
    // We can't fail after this point so this is a good time to provide a display frame index
    //

    ParsedFrameParameters->DisplayFrameIndex             = NextDisplayFrameIndex++;

    //
    // Manipulate the DTS to account for WMA's decoder latency.
    //
    // To avoid underflow the WMA decoder needs a generous 'float' of data
    // before it attempts to decode a frame. The maninitude of this float
    // must be encoded into the DTS otherwise the output timer will delay
    // issuing data for decode. 
    //

    const unsigned long long VeryEarlyDecodePorch = 250000;

    if( ParsedFrameParameters->NormalizedDecodeTime == INVALID_TIME )
	    ParsedFrameParameters->NormalizedDecodeTime = ParsedFrameParameters->NormalizedPlaybackTime;

    if( ParsedFrameParameters->NormalizedDecodeTime > ParsedFrameParameters->NormalizedPlaybackTime )
    {
	FRAME_ERROR("DTS(%lldus) > PTS(%lldus)!!!\n",
		    ParsedFrameParameters->NormalizedDecodeTime,
		    ParsedFrameParameters->NormalizedPlaybackTime);
	ParsedFrameParameters->NormalizedDecodeTime = ParsedFrameParameters->NormalizedPlaybackTime;
    }

    if( ValidTime ( ParsedFrameParameters->NormalizedDecodeTime ) &&
	( ParsedFrameParameters->NormalizedDecodeTime > VeryEarlyDecodePorch ) )
    {
	ParsedFrameParameters->NormalizedDecodeTime -= VeryEarlyDecodePorch;
	LastNormalizedDecodeTime = ParsedFrameParameters->NormalizedDecodeTime;
    }
    else
    {
	ParsedFrameParameters->NormalizedDecodeTime = LastNormalizedDecodeTime;
    }

    //
    // Use the super-class utilities to complete our housekeeping chores
    //

    HandleUpdateStreamParameters();

    //
    // *Don't* call GenerateNextFrameNormalizedPlaybackTime() here since it is meaningless
    // for WMA streams. This means that LastNormalizedPlaybackTime and
    // NextFrameNormalizedPlaybackTime will always be UNSPECIFIED_TIME.
    // 

    return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for WMA audio.
///
/// \copydoc FrameParser_Audio_c::PrepareReferenceFrameList()
///
FrameParserStatus_t   FrameParser_AudioWma_c::PrepareReferenceFrameList( void )
{
    return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for WMA audio.
///
/// \copydoc FrameParser_Audio_c::PrepareReferenceFrameList()
///
FrameParserStatus_t   FrameParser_AudioWma_c::UpdateReferenceFrameList( void )
{
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for WMA audio.
///
/// \copydoc FrameParser_Audio_c::ProcessReverseDecodeUnsatisfiedReferenceStack()
///
FrameParserStatus_t   FrameParser_AudioWma_c::ProcessReverseDecodeUnsatisfiedReferenceStack( void )
{
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for WMA audio.
///
/// \copydoc FrameParser_Audio_c::ProcessReverseDecodeStack()
///
FrameParserStatus_t   FrameParser_AudioWma_c::ProcessReverseDecodeStack(                        void )
{
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for WMA audio.
///
/// \copydoc FrameParser_Audio_c::PurgeReverseDecodeUnsatisfiedReferenceStack()
///
FrameParserStatus_t   FrameParser_AudioWma_c::PurgeReverseDecodeUnsatisfiedReferenceStack(      void )
{

    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for WMA audio.
///
/// \copydoc FrameParser_Audio_c::PurgeReverseDecodeStack()
///
FrameParserStatus_t   FrameParser_AudioWma_c::PurgeReverseDecodeStack(                  void )
{
    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for WMA audio.
///
/// \copydoc FrameParser_Audio_c::TestForTrickModeFrameDrop()
///
FrameParserStatus_t   FrameParser_AudioWma_c::TestForTrickModeFrameDrop(                        void )
{
    return FrameParserNoError;
}


