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

Source file name : frame_parser_audio_rma.cpp
Author :           Julian

Implementation of the Real Media Audio frame parser class for player 2.


Date        Modification                                Name
----        ------------                                --------
27-Jan-2009 Created                                     Julian

************************************************************************/


// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "frame_parser_audio_rma.h"
#include "ring_generic.h"

//#define DUMP_HEADERS

//{{{  Locally defined constants and macros
#undef FRAME_TAG
#define FRAME_TAG "Rma audio frame parser"

static BufferDataDescriptor_t     RmaAudioStreamParametersBuffer       = BUFFER_RMA_AUDIO_STREAM_PARAMETERS_TYPE;
static BufferDataDescriptor_t     RmaAudioFrameParametersBuffer        = BUFFER_RMA_AUDIO_FRAME_PARAMETERS_TYPE;

#define CodeToInteger(a,b,c,d)          ((a << 0) | (b << 8) | (c << 16) | (d << 24))

static inline unsigned int BE2LE (unsigned int Value)
{
    return (((Value&0xff)<<24) | ((Value&0xff00)<<8) | ((Value>>8)&0xff00) | ((Value>>24)&0xff));
}
//}}}

//{{{  Constructor
// /////////////////////////////////////////////////////////////////////////
//
//      Constructor
//

FrameParser_AudioRma_c::FrameParser_AudioRma_c( void )
{
    Configuration.FrameParserName               = "AudioRma";

    Configuration.StreamParametersCount         = 32;
    Configuration.StreamParametersDescriptor    = &RmaAudioStreamParametersBuffer;

    Configuration.FrameParametersCount          = 32;
    Configuration.FrameParametersDescriptor     = &RmaAudioFrameParametersBuffer;

    StreamFormatInfoValid                       = false;

    Reset();
}
//}}}
//{{{  Destructor
// /////////////////////////////////////////////////////////////////////////
//
//      Destructor
//

FrameParser_AudioRma_c::~FrameParser_AudioRma_c (void)
{
    Halt();
    Reset();
}
//}}}

//{{{  Reset
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      The Reset function release any resources, and reset all variable
///
/// /////////////////////////////////////////////////////////////////////////

FrameParserStatus_t   FrameParser_AudioRma_c::Reset (void )
{
    return FrameParser_Audio_c::Reset();
}
//}}}
//{{{  RegisterOutputBufferRing
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Register the output ring
///
FrameParserStatus_t   FrameParser_AudioRma_c::RegisterOutputBufferRing (Ring_t          Ring)
{
    // Clear our parameter pointers
    StreamParameters                    = NULL;
    FrameParameters                     = NULL;

    // Set illegal state forcing a parameter update on the first frame
    memset (&CurrentStreamParameters, 0, sizeof(CurrentStreamParameters));

    // Pass the call down the line
    return FrameParser_Audio_c::RegisterOutputBufferRing( Ring );
}
//}}}

//{{{  ResetReferenceFrameList
// /////////////////////////////////////////////////////////////////////////
//
//      The reset reference frame list function
//
FrameParserStatus_t   FrameParser_AudioRma_c::ResetReferenceFrameList( void )
{
    FRAME_DEBUG(">><<");

    Player->CallInSequence( Stream, SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, CODEC_RELEASE_ALL );

    return FrameParserNoError;
}
//}}}
//{{{  PrepareReferenceFrameList
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Not required for Rma audio
///
FrameParserStatus_t   FrameParser_AudioRma_c::PrepareReferenceFrameList (void)
{
    return FrameParserNoError;
}
//}}}
//{{{  UpdateReferenceFrameList
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Not required for Rma audio
///
FrameParserStatus_t   FrameParser_AudioRma_c::UpdateReferenceFrameList (void)
{
    return FrameParserNoError;
}
//}}}

//{{{  ProcessReverseDecodeStack
// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Not required for Rma audio
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_AudioRma_c::ProcessReverseDecodeStack (void)
{
    return FrameParserNoError;
}
//}}}
//{{{  ProcessReverseDecodeUnsatisfiedReferenceStack
// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Not required for Rma audio
///
FrameParserStatus_t   FrameParser_AudioRma_c::ProcessReverseDecodeUnsatisfiedReferenceStack (void)
{
    return FrameParserNoError;
}
//}}}
//{{{  PurgeReverseDecodeUnsatisfiedReferenceStack
// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Not required for Rma audio
///
FrameParserStatus_t   FrameParser_AudioRma_c::PurgeReverseDecodeUnsatisfiedReferenceStack(     void )
{

    return FrameParserNoError;
}
//}}}
//{{{  PurgeReverseDecodeStack
// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Not required for Rma audio
///
FrameParserStatus_t   FrameParser_AudioRma_c::PurgeReverseDecodeStack (void)
{

    return FrameParserNoError;
}
//}}}

//{{{  PurgeQueuedPostDecodeParameterSettings
///////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for Real Audio.
///
/// \copydoc FrameParser_Audio_c::PurgeQueuedPostDecodeParameterSettings()
///
FrameParserStatus_t   FrameParser_AudioRma_c::PurgeQueuedPostDecodeParameterSettings( void )
{
    return FrameParserNoError;
}
//}}}
//{{{  ProcessQueuedPostDecodeParameterSettings
///////////////////////////////////////////////////////////////////////////
///
/// Not required or implemented for Real Audio.
///
/// \copydoc FrameParser_Audio_c::ProcessQueuedPostDecodeParameterSettings()
///
FrameParserStatus_t   FrameParser_AudioRma_c::ProcessQueuedPostDecodeParameterSettings( void )
{
    return FrameParserNoError;
}
//}}}
//{{{  GeneratePostDecodeParameterSettings
////////////////////////////////////////////////////////////////////////////
///
/// Determine the display frame index and presentation time of the decoded frame.
///
/// For Real Audio these can be determined immediately (although it the first
/// frame for decode does not contain a PTS we must synthesize one).
///
FrameParserStatus_t   FrameParser_AudioRma_c::GeneratePostDecodeParameterSettings( void )
{
    FrameParserStatus_t Status;

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

    // Sythesize the presentation time if required
    Status                                              = HandleCurrentFrameNormalizedPlaybackTime();
    if (Status != FrameParserNoError)
        return Status;

    // We can't fail after this point so this is a good time to provide a display frame index
    ParsedFrameParameters->DisplayFrameIndex             = NextDisplayFrameIndex++;

    // Use the super-class utilities to complete our housekeeping chores
    HandleUpdateStreamParameters();
    GenerateNextFrameNormalizedPlaybackTime (CurrentStreamParameters.SamplesPerFrame,  CurrentStreamParameters.SampleRate);

    return FrameParserNoError;
}
//}}}

//{{{  TestForTrickModeFrameDrop
// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Not required for Rma audio
///
FrameParserStatus_t   FrameParser_AudioRma_c::TestForTrickModeFrameDrop (void)
{

    return FrameParserNoError;
}
//}}}

//{{{  ReadHeaders
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Scan the start code list reading header specific information
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_AudioRma_c::ReadHeaders (void)
{
    FrameParserStatus_t         Status;

#if 0
    unsigned int                i;
    report (severity_info, "Buffer (%d) :", BufferLength);
    for (i=0; i<BufferLength; i++)
    {
        report (severity_info, "%02x ", BufferData[i]);
        if (((i+1)&0x1f)==0)
            report (severity_info, "\n");
    }
    report (severity_info, "\n");
#endif

    Bits.SetPointer (BufferData);
    DataOffset                  = 0;

    // Perform the common portion of the read headers function
    FrameParser_Audio_c::ReadHeaders();

    if (!StreamFormatInfoValid)
    {
        Status                  = GetNewStreamParameters ((void **)&StreamParameters);
        if (Status != FrameParserNoError)
        {
            FRAME_ERROR ("Cannot get new stream parameters\n");
            return Status;
        }

        Status                  = ReadStreamParameters();
        if (Status != FrameParserNoError)
        {
            FRAME_ERROR ("Failed to parse stream parameters\n" );
            return Status;
        }

        memcpy (&CurrentStreamParameters, StreamParameters, sizeof(CurrentStreamParameters));
        StreamFormatInfoValid   = true;
    }

    Status = GetNewFrameParameters ((void**)&FrameParameters);
    if (Status != FrameParserNoError)
    {
        FRAME_ERROR ("Cannot get new frame parameters\n");
        return Status;
    }

    // Nick inserted some default values here
    ParsedFrameParameters->FirstParsedParametersForOutputFrame          = true;
    ParsedFrameParameters->FirstParsedParametersAfterInputJump          = FirstDecodeAfterInputJump;
    ParsedFrameParameters->SurplusDataInjected                          = SurplusDataInjected;
    ParsedFrameParameters->ContinuousReverseJump                        = ContinuousReverseJump;
    ParsedFrameParameters->KeyFrame                                     = true;
    ParsedFrameParameters->ReferenceFrame                               = false;

    ParsedFrameParameters->NewFrameParameters                           = true;
    ParsedFrameParameters->SizeofFrameParameterStructure                = sizeof(RmaAudioFrameParameters_t);
    ParsedFrameParameters->FrameParameterStructure                      = FrameParameters;

    FrameParameters->FrameSize                                          = CurrentStreamParameters.SubPacketSize;
    FrameParameters->BitRate                                            = (CurrentStreamParameters.SubPacketSize * CurrentStreamParameters.SampleRate * 8) /
                                                                           CurrentStreamParameters.SamplesPerFrame;

    ParsedAudioParameters->Source.BitsPerSample                         = CurrentStreamParameters.SampleSize;
    ParsedAudioParameters->Source.ChannelCount                          = CurrentStreamParameters.ChannelCount;
    ParsedAudioParameters->Source.SampleRateHz                          = CurrentStreamParameters.SampleRate;
    ParsedAudioParameters->SampleCount                                  = CurrentStreamParameters.SamplesPerFrame;

    ParsedFrameParameters->DataOffset                                   = DataOffset;

    FrameToDecode                                                       = true;

    return FrameParserNoError;

}
//}}}
//{{{  ReadStreamParameters
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Read in a FormatInfo structure
///
/// /////////////////////////////////////////////////////////////////////////

FrameParserStatus_t   FrameParser_AudioRma_c::ReadStreamParameters (void)
{
    unsigned int                Unused;
    unsigned char*              StartPointer;
    unsigned char*              EndPointer;
    unsigned int                BitsInByte;

    FRAME_DEBUG("\n");

#if 0
    unsigned int        Checksum = 0;
    report (severity_info, "data %d :\n", BufferLength);
    for (int i=0; i<BufferLength; i++)
    {
        if ((i&0x0f)==0)
            report (severity_info, "\n%06x", i);
        report (severity_info, " %02x", BufferData[i]);
        Checksum       += BufferData[i];
    }
    report (severity_info, "\nChecksum %08x\n", Checksum);
#endif

    Bits.GetPosition (&StartPointer, &BitsInByte);

    StreamParameters->Length                    = Bits.Get(32);
    StreamParameters->HeaderSignature           = BE2LE(Bits.Get(32));
    StreamParameters->Version                   = Bits.Get(16);
    if (StreamParameters->Version == 3)
    {
        StreamParameters->HeaderSize            = Bits.Get(32);
    }
    else
    {
        Unused                                  = Bits.Get(16);
        StreamParameters->RaSignature           = BE2LE(Bits.Get(32));
        StreamParameters->Size                  = Bits.Get(32);
        StreamParameters->Version2              = Bits.Get(16);
        StreamParameters->HeaderSize            = Bits.Get(32);
        StreamParameters->CodecFlavour          = Bits.Get(16);
        StreamParameters->CodedFrameSize        = Bits.Get(32);

        Bits.Get(32);
        Bits.Get(32);
        Bits.Get(32);

        StreamParameters->SubPacket             = Bits.Get(16);
        StreamParameters->FrameSize             = Bits.Get(16);
        StreamParameters->SubPacketSize         = Bits.Get(16);

        Bits.Get(16);

        if (StreamParameters->Version == 5)
        {
            Bits.Get(32);
            Bits.Get(16);
        }
        StreamParameters->SampleRate            = Bits.Get(16);

        Bits.Get(16);
        StreamParameters->SampleSize            = Bits.Get(16);
        StreamParameters->ChannelCount          = Bits.Get(16);
        if (StreamParameters->Version == 4)
        {
            Bits.Get(8);
            StreamParameters->InterleaverId     = BE2LE(Bits.Get(32));
            Bits.Get(8);
            StreamParameters->CodecId           = BE2LE(Bits.Get(32));
        }
        else
        {
            StreamParameters->InterleaverId     = BE2LE(Bits.Get(32));
            StreamParameters->CodecId           = BE2LE(Bits.Get(32));
        }
//{{{  COMMENT
#if 0
    FormatInfo->Length                      = ReadObject32 (RmfFile);
    fread (&FormatInfo->HeaderSignature, 1, 4, RmfFile);
    FormatInfo->Version                 = ReadObject16 (RmfFile);
        Unused                          = ReadObject16 (RmfFile);
        fread (&FormatInfo->RaSignature, 1, 4, RmfFile);
        FormatInfo->Size                = ReadObject32 (RmfFile);
        FormatInfo->Version2            = ReadObject16 (RmfFile);
        FormatInfo->HeaderSize          = ReadObject32 (RmfFile);
        FormatInfo->CodecFlavour        = ReadObject16 (RmfFile);
        FormatInfo->CodedFrameSize      = ReadObject32 (RmfFile);
        fseeko (RmfFile, 12, SEEK_CUR);
        FormatInfo->SubPacket           = ReadObject16 (RmfFile);
        FormatInfo->FrameSize           = ReadObject16 (RmfFile);
        FormatInfo->SubPacketSize       = ReadObject16 (RmfFile);
        fseeko (RmfFile, 2, SEEK_CUR);
        if (FormatInfo->Version == 5)
            fseeko (RmfFile, 6, SEEK_CUR);
        FormatInfo->SampleRate          = ReadObject16 (RmfFile);
        fseeko (RmfFile, 2, SEEK_CUR);
        FormatInfo->SampleSize          = ReadObject16 (RmfFile);
        FormatInfo->ChannelCount        = ReadObject16 (RmfFile);
        if (FormatInfo->Version == 4)
        {
            fgetc (RmfFile);
            fread (&FormatInfo->InterleaverId, 1, 4, RmfFile);
            fgetc (RmfFile);
            fread (&FormatInfo->CodecId, 1, 4, RmfFile);
        }
#endif
//}}}

        Bits.Get(24);

        if (StreamParameters->Version == 5)
            Bits.Get(8);

        StreamParameters->CodecOpaqueDataLength  = Bits.Get(32);

        Bits.GetPosition (&EndPointer, &BitsInByte);
        DataOffset                               = (unsigned int)EndPointer - (unsigned int)StartPointer;

#if 0
        FRAME_TRACE("%s: Data %p, Length %d\n", __FUNCTION__, EndPointer, BufferLength-DataOffset);
        report (severity_info, "Data\n");
        for (int i=DataOffset; i<BufferLength; i++)
        {
            report (severity_info, "%02x ", BufferData[i]);
            if (((i+1-DataOffset)&0x1f)==0)
                report (severity_info, "\n");
        }
        report (severity_info, "\n");
#endif

        switch (StreamParameters->CodecId)
        {
            case CodeToInteger('d','n','e','t'):                /* AC3 */
                break;
            case CodeToInteger('s','i','p','r'):                /* Sipro */
                break;
            case CodeToInteger('2','8','_','8'):                /* Real Audio 2.0 */
                break;
            case CodeToInteger('c','o','o','k'):                /* Rma */
            case CodeToInteger('a','t','r','c'):                /* ATRAC */
            {
                StreamParameters->RmaVersion            = Bits.Get(32);
                StreamParameters->SamplesPerFrame       = Bits.Get(16) / StreamParameters->ChannelCount;

                break;
            }
            case CodeToInteger('r','a','a','c'):                /* LC-AAC */
            case CodeToInteger('r','a','c','p'):                /* HE-AAC */
                break;
            case CodeToInteger('r','a','l','f'):                /* Real Audio Lossless */
                break;
        }
    }

#ifdef DUMP_HEADERS
    report (severity_info, "StreamFormatInfo :- \n");
    report (severity_info, "Length                      %6u\n", StreamParameters->Length);
    report (severity_info, "HeaderSignature               %.4s\n", (unsigned char*)&StreamParameters->HeaderSignature);
    report (severity_info, "Version                     %6u\n", StreamParameters->Version);
    report (severity_info, "RaSignature                   %.4s\n", (unsigned char*)&StreamParameters->RaSignature);
    report (severity_info, "Size                        %6u\n", StreamParameters->Size);
    report (severity_info, "Version2                    %6u\n", StreamParameters->Version2);
    report (severity_info, "HeaderSize                  %6u\n", StreamParameters->HeaderSize);
    report (severity_info, "CodecFlavour                %6u\n", StreamParameters->CodecFlavour);
    report (severity_info, "CodedFrameSize              %6u\n", StreamParameters->CodedFrameSize);
    report (severity_info, "SubPacket                   %6u\n", StreamParameters->SubPacket);
    report (severity_info, "FrameSize                   %6u\n", StreamParameters->FrameSize);
    report (severity_info, "SubPacketSize               %6u\n", StreamParameters->SubPacketSize);
    report (severity_info, "SampleRate                  %6u\n", StreamParameters->SampleRate);
    report (severity_info, "SampleSize                  %6u\n", StreamParameters->SampleSize);
    report (severity_info, "ChannelCount                %6u\n", StreamParameters->ChannelCount);
    report (severity_info, "InterleaverId                 %.4s\n", (unsigned char*)&StreamParameters->InterleaverId);
    report (severity_info, "CodecId                       %.4s\n", (unsigned char*)&StreamParameters->CodecId);
    report (severity_info, "CodecOpaqueDataLength       %6u\n", StreamParameters->CodecOpaqueDataLength);
    report (severity_info, "DataOffset                  %6u\n", DataOffset);
    report (severity_info, "RmaVersion             0x%08x\n", StreamParameters->RmaVersion);
    report (severity_info, "SamplesPerFrame             %6u\n", StreamParameters->SamplesPerFrame);

#endif

    return FrameParserNoError;
}
//}}}

