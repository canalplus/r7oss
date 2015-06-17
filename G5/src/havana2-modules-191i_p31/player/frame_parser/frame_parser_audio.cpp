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

Source file name : frame_parser_audio.cpp
Author :           Daniel

Implementation of the base audio frame parser class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
30-Mar-07   Created (from frame_parser_video.cpp)           Daniel

************************************************************************/

// /////////////////////////////////////////////////////////////////////
//
//	Include any component headers

#include "frame_parser_audio.h"
#include "st_relay.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

#undef FRAME_TAG
#define FRAME_TAG "Audio frame parser"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

///////////////////////////////////////////////////////////////////////////
///
///     Constructor
///
FrameParser_Audio_c::FrameParser_Audio_c()
{
    PtsJitterTollerenceThreshold = 1000;		// Changed to 1ms by nick, because some file formats specify pts times to 1 ms accuracy

//

    Reset();
}

// /////////////////////////////////////////////////////////////////////////
//
// 	The Reset function release any resources, and reset all variable
//

FrameParserStatus_t   FrameParser_Audio_c::Reset(	void )
{
    ParsedAudioParameters	= NULL;

    LastNormalizedPlaybackTime               = UNSPECIFIED_TIME;
    NextFrameNormalizedPlaybackTime          = UNSPECIFIED_TIME;
    NextFramePlaybackTimeAccumulatedError    = 0;
    
    UpdateStreamParameters              = false;
    
    return FrameParser_Base_c::Reset();
}


// /////////////////////////////////////////////////////////////////////////
//
//      The register output ring function
//

FrameParserStatus_t   FrameParser_Audio_c::RegisterOutputBufferRing(
					Ring_t		Ring )
{
FrameParserStatus_t	Status;

    //
    // First allow the base class to perform it's operations, 
    // as we operate on the buffer pool obtained by it.
    //

    Status	= FrameParser_Base_c::RegisterOutputBufferRing( Ring );
    if( Status != FrameParserNoError )
	return Status;

    //
    // Attach the audio specific parsed frame parameters to every element of the pool
    //

    Status      = CodedFrameBufferPool->AttachMetaData( Player->MetaDataParsedAudioParametersType );
    if( Status != BufferNoError )
    {
	report( severity_error, "FrameParser_Audio_c::RegisterCodedFrameBufferPool - Failed to attach parsed audio parameters to all coded frame buffers.\n" );
	SetComponentState(ComponentInError);
        return Status;
    }

//

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
// 	The input function perform audio specific operations
//

FrameParserStatus_t   FrameParser_Audio_c::Input(	Buffer_t		  CodedBuffer )
{
FrameParserStatus_t	Status;

    //
    // Are we allowed in here
    //

    AssertComponentState( "FrameParser_Audio_c::Input", ComponentRunning );

    //
    // Initialize context pointers
    //

    ParsedAudioParameters	= NULL;

    //
    // First perform base operations
    //

    Status	= FrameParser_Base_c::Input( CodedBuffer );
    if( Status != FrameParserNoError )
	return Status;

    st_relayfs_write(ST_RELAY_TYPE_CODED_AUDIO_BUFFER, ST_RELAY_SOURCE_AUDIO_FRAME_PARSER, (unsigned char *)BufferData, BufferLength, 0 );
    
    //
    // Obtain audio specific pointers to data associated with the buffer.
    //

    Status	= Buffer->ObtainMetaDataReference( Player->MetaDataParsedAudioParametersType, (void **)(&ParsedAudioParameters) );
    if( Status != PlayerNoError )
    {
	report( severity_error, "FrameParser_Audio_c::Input - Unable to obtain the meta data \"ParsedVideoParameters\".\n" );
	return Status;
    }

    memset( ParsedAudioParameters, 0x00, sizeof(ParsedAudioParameters_t) );

    //
    // Now execute the processing chain for a buffer
    //

    return ProcessBuffer();
}

// /////////////////////////////////////////////////////////////////////////
///
///     \brief Common portion of read headers
///       
///     Handle invalidation of what we knew about time on a discontinuity
///
///     \return Frame parser status code, FrameParserNoError indicates success.
///

FrameParserStatus_t   FrameParser_Audio_c::ReadHeaders(          void )
{
    if( FirstDecodeAfterInputJump && !ContinuousReverseJump )
    {
	LastNormalizedPlaybackTime               = UNSPECIFIED_TIME;
	NextFrameNormalizedPlaybackTime          = UNSPECIFIED_TIME;
	NextFramePlaybackTimeAccumulatedError    = 0;
    }

    return FrameParserNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Ensure the current frame has a suitable (normalized) playback time.
///
/// If the frame was provided with a genuine timestamp then this will be used and
/// the information stored to be used in any future timestamp synthesis.
///
/// If the frame was not provided with a timestamp then the time previously
/// calculated by FrameParser_Audio_c::GenerateNextFrameNormalizedPlaybackTime
/// will be used. If no time has been previously recorded then the method fails.
///
/// \return FrameParserNoError if a timestamp was present or could be synthesised, FrameParserError otherwise.
///    
FrameParserStatus_t FrameParser_Audio_c::HandleCurrentFrameNormalizedPlaybackTime()
{
    FrameParserStatus_t Status;
    
    if( ParsedFrameParameters->NormalizedPlaybackTime == INVALID_TIME )
    {
        ParsedFrameParameters->NormalizedPlaybackTime = NextFrameNormalizedPlaybackTime;
        
        FRAME_DEBUG( "Using synthetic PTS for frame %d: %lluus (delta %lldus)\n",
        	     NextDecodeFrameIndex,
                     ParsedFrameParameters->NormalizedPlaybackTime,
                     ParsedFrameParameters->NormalizedPlaybackTime - LastNormalizedPlaybackTime );

        if ( ValidTime( ParsedFrameParameters->NormalizedPlaybackTime ) )
        {
            Status = TranslatePlaybackTimeNormalizedToNative( NextFrameNormalizedPlaybackTime,
                                                          &ParsedFrameParameters->NativePlaybackTime );
            /* Non-fatal error. Having no native timestamp does not harm the player but may harm the values it
             * reports to applications (e.g. get current PTS).
             */
            if( Status != FrameParserNoError )
        	FRAME_ERROR( "Cannot translate synthetic timestamp to native time\n" );
        }
        else
        {
            ParsedFrameParameters->NativePlaybackTime = ParsedFrameParameters->NormalizedPlaybackTime;
        }
    }
    else
    {
    	// reset the accumulated error
    	NextFramePlaybackTimeAccumulatedError = 0;

    	FRAME_DEBUG( "Using real PTS for frame %d:      %lluus (delta %lldus)\n",
    		     NextDecodeFrameIndex,
    	             ParsedFrameParameters->NormalizedPlaybackTime,
    	             ParsedFrameParameters->NormalizedPlaybackTime - LastNormalizedPlaybackTime );

        // Squawk if time does not progress quite as expected.
        if ( LastNormalizedPlaybackTime != UNSPECIFIED_TIME )
        {
            long long RealDelta = ParsedFrameParameters->NormalizedPlaybackTime - LastNormalizedPlaybackTime;
            long long SyntheticDelta = NextFrameNormalizedPlaybackTime - LastNormalizedPlaybackTime;
            long long DeltaDelta = RealDelta - SyntheticDelta;
            
            // Check that the predicted and actual times deviate by no more than the threshold
            if (DeltaDelta < -PtsJitterTollerenceThreshold || DeltaDelta > PtsJitterTollerenceThreshold) 
            {
    	        FRAME_ERROR( "Unexpected change in playback time. Expected %lldus, got %lldus (deltas: exp. %lld  got %lld )\n",
    	                     NextFrameNormalizedPlaybackTime, ParsedFrameParameters->NormalizedPlaybackTime,
                             SyntheticDelta, RealDelta);
    	        

            }            
        }        
    }

    return FrameParserNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Handle the management of stream parameters in a uniform way.
///
/// The frame parser has an unexpected requirement imposed upon it; every
/// frame must have the stream parameters attached to it (in case frames
/// are discarded between parsing and decoding). This method handles the
/// emission of stream parameters in a uniform way.
///
/// It should be used by any (audio) frame parser that supports stream
/// parameters.
///
void FrameParser_Audio_c::HandleUpdateStreamParameters()
{
BufferStatus_t Status;
void *StreamParameters;

    if( NULL == StreamParametersBuffer )
    {
	FRAME_ERROR( "Cannot handle NULL stream parameters\n" );
	return;
    }

    //
    // If this frame contains a new set of stream parameters mark this as such for the players
    // attention.
    //
    
    if( UpdateStreamParameters )
    {
    	// the framework automatically zeros this structure (i.e. we need not ever set to false)
    	ParsedFrameParameters->NewStreamParameters = true;
        UpdateStreamParameters = false;
    }
    
    //
    // Unconditionally send the stream parameters down the chain.
    //
    
    Status      = StreamParametersBuffer->ObtainDataReference( NULL, NULL, &StreamParameters );
    if( BufferNoError == Status )
    {
	ParsedFrameParameters->SizeofStreamParameterStructure = FrameParametersDescriptor->FixedSize;
	ParsedFrameParameters->StreamParameterStructure = StreamParameters;
	Buffer->AttachBuffer(StreamParametersBuffer);
    }
    else
    {
	FRAME_ASSERT(0); // we know that GetNewStreamParameters() was successful, failure is 'impossible'
    }
}

////////////////////////////////////////////////////////////////////////////
///
/// Calculate the expected PTS of the next frame. 
///
/// This will be used to synthesis a presentation time if this is missing from
/// the subsequent frame. It could, optionally, be used by some frame parsers to
/// identify unexpected time discontinuities.
///    
void FrameParser_Audio_c::GenerateNextFrameNormalizedPlaybackTime(
		unsigned int SampleCount, unsigned SamplingFrequency)
{
unsigned long long FrameDuration;

    FRAME_ASSERT(SampleCount && SamplingFrequency);

    if( ParsedFrameParameters->NormalizedPlaybackTime == UNSPECIFIED_TIME )
        return;
//

    LastNormalizedPlaybackTime = ParsedFrameParameters->NormalizedPlaybackTime;
    FrameDuration = ((unsigned long long) SampleCount * 1000000ull) / (unsigned long long) SamplingFrequency;
    NextFrameNormalizedPlaybackTime = ParsedFrameParameters->NormalizedPlaybackTime + FrameDuration;
    NextFramePlaybackTimeAccumulatedError += (SampleCount * 1000000ull) - (FrameDuration * SamplingFrequency);
    
    if ( NextFramePlaybackTimeAccumulatedError > ((unsigned long long) SamplingFrequency * 1000000ull) )
    {
    	NextFrameNormalizedPlaybackTime++;
    	NextFramePlaybackTimeAccumulatedError -= (unsigned long long) SamplingFrequency * 1000000ull;
    }
}

