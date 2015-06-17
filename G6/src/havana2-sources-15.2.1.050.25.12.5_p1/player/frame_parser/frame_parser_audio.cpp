/************************************************************************
Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine.

Streaming Engine is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Streaming Engine Library may alternatively be licensed under a
proprietary license from ST.
************************************************************************/

#include "st_relayfs_se.h"
#include "frame_parser_audio.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_Audio_c"

///////////////////////////////////////////////////////////////////////////
///
///     Constructor
///
FrameParser_Audio_c::FrameParser_Audio_c()
    : FrameParser_Base_c()
    , ParsedAudioParameters(NULL)
    , PtsJitterTollerenceThreshold(1000)  // some file formats specify pts times to 1 ms accuracy
    , LastNormalizedPlaybackTime(UNSPECIFIED_TIME)
    , NextFrameNormalizedPlaybackTime(UNSPECIFIED_TIME)
    , NextFramePlaybackTimeAccumulatedError(0)
    , UpdateStreamParameters(false)
    , RelayfsIndex()
{
    SetGroupTrace(group_frameparser_audio);

    RelayfsIndex = st_relayfs_getindex_fortype_se(ST_RELAY_TYPE_CODED_AUDIO_BUFFER);
}

////////////////////////////////////////////////////////////////////////////
///      Destructor
////
FrameParser_Audio_c::~FrameParser_Audio_c()
{
    st_relayfs_freeindex_fortype_se(ST_RELAY_TYPE_CODED_AUDIO_BUFFER, RelayfsIndex);
}

// /////////////////////////////////////////////////////////////////////////
//
//      Method to connect to neighbor
//

FrameParserStatus_t   FrameParser_Audio_c::Connect(Port_c *Port)
{
    FrameParserStatus_t Status;
    //
    // First allow the base class to perform it's operations,
    // as we operate on the buffer pool obtained by it.
    //
    Status  = FrameParser_Base_c::Connect(Port);

    if (Status != FrameParserNoError)
    {
        return Status;
    }

    //
    // Attach the audio specific parsed frame parameters to every element of the pool
    //
    Status = CodedFrameBufferPool->AttachMetaData(Player->MetaDataParsedAudioParametersType);
    if (Status != BufferNoError)
    {
        SE_ERROR("Failed to attach parsed audio parameters to all coded frame buffers\n");
        SetComponentState(ComponentInError);
        return Status;
    }

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//  The input function perform audio specific operations
//

FrameParserStatus_t   FrameParser_Audio_c::Input(Buffer_t         CodedBuffer)
{
    FrameParserStatus_t Status;
    //
    // Are we allowed in here
    //
    AssertComponentState(ComponentRunning);
    //
    // Initialize context pointers
    //
    ParsedAudioParameters   = NULL;
    //
    // First perform base operations
    //
    Status  = FrameParser_Base_c::Input(CodedBuffer);
    if (Status != FrameParserNoError)
    {
        return Status;
    }

    //Audio Descriptor info fill in ParsedFrameParameters
    if (CodedFrameParameters->ADMetaData.ADInfoAvailable)
    {
        memcpy(&ParsedFrameParameters->ADMetaData, &CodedFrameParameters->ADMetaData, sizeof(ADMetaData_t));
    }

    st_relayfs_write_se(ST_RELAY_TYPE_CODED_AUDIO_BUFFER + RelayfsIndex, ST_RELAY_SOURCE_SE,
                        (unsigned char *)BufferData, BufferLength, 0);
    //
    // Obtain audio specific pointers to data associated with the buffer.
    //
    Buffer->ObtainMetaDataReference(Player->MetaDataParsedAudioParametersType, (void **)(&ParsedAudioParameters));
    SE_ASSERT(ParsedAudioParameters != NULL);

    memset(ParsedAudioParameters, 0, sizeof(ParsedAudioParameters_t));

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

FrameParserStatus_t   FrameParser_Audio_c::ReadHeaders(void)
{
    if (FirstDecodeAfterInputJump && !ContinuousReverseJump)
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
/// This function also deploys workarounds for broken streams that consistently
/// report the PTS as zero. In that case the PTS will be ignored and treated as
/// though it was invalid.
///
/// \return FrameParserNoError if a timestamp was present or could be synthesised, FrameParserError otherwise.
///
FrameParserStatus_t FrameParser_Audio_c::HandleCurrentFrameNormalizedPlaybackTime()
{
    if (NotValidTime(ParsedFrameParameters->NormalizedPlaybackTime))
    {
        HandleInvalidPlaybackTime();
    }
    else
    {
        // reset the accumulated error
        NextFramePlaybackTimeAccumulatedError = 0;
        SE_DEBUG(group_frameparser_audio,  "Using real PTS for frame %d:      %lluus (delta %lldus)\n",
                 NextDecodeFrameIndex,
                 ParsedFrameParameters->NormalizedPlaybackTime,
                 ParsedFrameParameters->NormalizedPlaybackTime - LastNormalizedPlaybackTime);

        // Squawk if time does not progress quite as expected.
        if (ValidTime(LastNormalizedPlaybackTime))
        {
            long long RealDelta = ParsedFrameParameters->NormalizedPlaybackTime - LastNormalizedPlaybackTime;
            long long SyntheticDelta = NextFrameNormalizedPlaybackTime - LastNormalizedPlaybackTime;
            long long DeltaDelta = RealDelta - SyntheticDelta;

            // Check that the predicted and actual times deviate by no more than the threshold
            if (DeltaDelta < -PtsJitterTollerenceThreshold || DeltaDelta > PtsJitterTollerenceThreshold)
            {
                //manage Null PTS case: bug13276
                // In that case, PTS is marked as invalid and predicted timestamp is used instead.
                if (ParsedFrameParameters->NativePlaybackTime == 0)
                {
                    SE_DEBUG(group_frameparser_audio,  "Null PTS detected for Frame %d\n", NextDecodeFrameIndex);
                    ParsedFrameParameters->NativePlaybackTime     = INVALID_TIME;
                    ParsedFrameParameters->NormalizedPlaybackTime = INVALID_TIME;
                    HandleInvalidPlaybackTime();
                }
                //standard case: time deviation error
                else
                {
                    if (Player->PolicyValue(Playback, Stream, PolicyAVDSynchronization) == PolicyValueApply)
                        SE_WARNING("Unexpected change in playback time. Expected %lldus, got %lldus (deltas: exp. %lld  got %lld )\n",
                                   NextFrameNormalizedPlaybackTime, ParsedFrameParameters->NormalizedPlaybackTime,
                                   SyntheticDelta, RealDelta);
                }
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
    void *StreamParameters;

    if (NULL == StreamParametersBuffer)
    {
        SE_ERROR("Cannot handle NULL stream parameters\n");
        return;
    }

    //
    // If this frame contains a new set of stream parameters mark this as such for the players
    // attention.
    //

    if (UpdateStreamParameters)
    {
        // the framework automatically zeros this structure (i.e. we need not ever set to false)
        ParsedFrameParameters->NewStreamParameters = true;
        UpdateStreamParameters = false;
    }

    //
    // Unconditionally send the stream parameters down the chain.
    //
    StreamParametersBuffer->ObtainDataReference(NULL, NULL, &StreamParameters);
    SE_ASSERT(StreamParameters != NULL);  // not supposed to be empty

    ParsedFrameParameters->SizeofStreamParameterStructure = FrameParametersDescriptor->FixedSize;
    ParsedFrameParameters->StreamParameterStructure = StreamParameters;
    Buffer->AttachBuffer(StreamParametersBuffer);
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
    SE_ASSERT(SampleCount && SamplingFrequency);

    if (NotValidTime(ParsedFrameParameters->NormalizedPlaybackTime))
    {
        return;
    }

    LastNormalizedPlaybackTime = ParsedFrameParameters->NormalizedPlaybackTime;
    FrameDuration = ((unsigned long long) SampleCount * 1000000ull) / (unsigned long long) SamplingFrequency;
    NextFrameNormalizedPlaybackTime = ParsedFrameParameters->NormalizedPlaybackTime + FrameDuration;
    NextFramePlaybackTimeAccumulatedError += (SampleCount * 1000000ull) - (FrameDuration * SamplingFrequency);

    if (NextFramePlaybackTimeAccumulatedError > ((unsigned long long) SamplingFrequency * 1000000ull))
    {
        NextFrameNormalizedPlaybackTime++;
        NextFramePlaybackTimeAccumulatedError -= (unsigned long long) SamplingFrequency * 1000000ull;
    }
}

////////////////////////////////////////////////////////////////////////////
///
/// Handle case of invalid Playback Time : null PTS also fall into this case
///
/// If the frame was not provided with a timestamp (and non null) then the time
/// previously calculated by FrameParser_Audio_c::GenerateNextFrameNormalizedPlaybackTime
/// will be used. If no time has been previously recorded then the method fails.
///
void FrameParser_Audio_c::HandleInvalidPlaybackTime()
{
    FrameParserStatus_t Status;
    ParsedFrameParameters->NormalizedPlaybackTime = NextFrameNormalizedPlaybackTime;
    SE_DEBUG(group_frameparser_audio,  "Using synthetic PTS for frame %d: %lluus (delta %lldus)\n",
             NextDecodeFrameIndex,
             ParsedFrameParameters->NormalizedPlaybackTime,
             ParsedFrameParameters->NormalizedPlaybackTime - LastNormalizedPlaybackTime);

    if (ValidTime(ParsedFrameParameters->NormalizedPlaybackTime))
    {
        Status = TranslatePlaybackTimeNormalizedToNative(NextFrameNormalizedPlaybackTime,
                                                         &ParsedFrameParameters->NativePlaybackTime);

        /* Non-fatal error. Having no native timestamp does not harm the player but may harm the values it
         * reports to applications (e.g. get current PTS).
         */
        if (Status != FrameParserNoError)
        {
            SE_ERROR("Cannot translate synthetic timestamp to native time\n");
        }
    }
    else
    {
        ParsedFrameParameters->NativePlaybackTime = ParsedFrameParameters->NormalizedPlaybackTime;
    }
}

