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

#ifndef H_CODEC
#define H_CODEC

#include "player.h"

enum
{
    CodecNoError        = PlayerNoError,
    CodecError          = PlayerError,

    CodecUnknownFrame   = BASE_CODEC
};

typedef PlayerStatus_t  CodecStatus_t;

enum
{
    CodecFnConnect = BASE_CODEC,
    CodecFnOutputPartialDecodeBuffers,
    CodecFnReleaseReferenceFrame,
    CodecFnReleaseDecodeBuffer,
    CodecFnInput,
    CodecFnDiscardQueuedDecodes,
};

#define CODEC_RELEASE_ALL       INVALID_INDEX

#define TUNEABLE_NAME_AUDIO_DECODER_ENABLE_CRC          "audio_decoder_enable_crc"
#define TUNEABLE_NAME_AUDIO_DECODER_UPMIX_MONO2STEREO   "audio_decoder_upmix_mono2stereo"

class Codec_c : public BaseComponentClass_c
{
public:
    // This variable can be set through debugfs to request the audio-coprocessor
    // to compute and report the CRC of its output
    static unsigned int volatile EnableAudioCRC;

    // This variable can be set through debugfs to request the audio-codec
    // to upmix mono streams to stereo
    static unsigned int volatile UpmixMono2Stereo;

    virtual CodecStatus_t   GetTrickModeParameters(CodecTrickModeParameters_t   *TrickModeParameters) = 0;

    virtual CodecStatus_t   Connect(Port_c *Port) = 0;

    virtual void            UpdateConfig(unsigned int Config) = 0;

    virtual CodecStatus_t   OutputPartialDecodeBuffers(void) = 0;

    virtual CodecStatus_t   DiscardQueuedDecodes(void) = 0;

    virtual CodecStatus_t   ReleaseReferenceFrame(unsigned int        ReferenceFrameDecodeIndex) = 0;

    virtual CodecStatus_t   CheckReferenceFrameList(unsigned int          NumberOfReferenceFrameLists,
                                                    ReferenceFrameList_t  ReferenceFrameList[]) = 0;

    virtual CodecStatus_t   ReleaseDecodeBuffer(Buffer_t Buffer) = 0;

    virtual CodecStatus_t   Input(Buffer_t CodedBuffer) = 0;

    // Low power methods
    virtual CodecStatus_t   LowPowerEnter(void) = 0;
    virtual CodecStatus_t   LowPowerExit(void) = 0;

    virtual CodecStatus_t   UpdatePlaybackSpeed(void) = 0;

    static void RegisterTuneable(void)
    {
        // Register the Decoder SW CRC option global
        OS_RegisterTuneable(TUNEABLE_NAME_AUDIO_DECODER_ENABLE_CRC       , (unsigned int *)&EnableAudioCRC);
        OS_RegisterTuneable(TUNEABLE_NAME_AUDIO_DECODER_UPMIX_MONO2STEREO, (unsigned int *)&UpmixMono2Stereo);
    }

    static void UnregisterTuneable(void)
    {
        // Register the Decoder SW CRC option global
        OS_UnregisterTuneable(TUNEABLE_NAME_AUDIO_DECODER_ENABLE_CRC);
        OS_UnregisterTuneable(TUNEABLE_NAME_AUDIO_DECODER_UPMIX_MONO2STEREO);
    }
};

// ---------------------------------------------------------------------
//
// Documentation
//

/*! \class Codec_c
\brief Responsible for taking individual parsed coded frames and decoding them.

The codec class is responsible for taking individual parsed coded
frames and decoding them. It manages the decode buffers, and when
decode buffer is complete (all fields + all slices) it places the
decode buffer on its output ring. This is a list of its entrypoints,
and a partial list of the calls it makes, and the data structures it
accesses, these are in addition to the standard component class
entrypoints, and the complete list of support entrypoints in the Player
class.

The partial list of entrypoints used by this class:

- Empty list

The partial list of meta data types used by this class:

- Attached to input buffers:
  - <b>ParsedFrameParameters</b>, Describes the coded frame to be decoded.
  - <b>StartCodeList</b>, Optional input for those codecs that require a list of start codes (IE mpeg2 soft decoder).
  - <b>Parsed[Video|Audio|Data]Parameters</b>, Optional input attachment to be transferred to the output.

- Added to output buffers:
  - <b>ParsedFrameParameters</b>, Copied from the input, or merged with any current data for video decoding of partial frames.
  - <b>Parsed[Video|Audio|Data]Parameters</b>, Optional Copied from the input, or merged with any current data for video decoding of partial frames.

*/

/*! \fn CodecStatus_t   GetTrickModeParameters(     CodecTrickModeParameters_t      *TrickModeParameters )
\brief Gain access to parameters describing trick mode capabilities.

This function gains a copy of a datastructuture held within the codec, that
describes the abilities, of the codec, with respect to supporting trick modes.
In particular it supplies information that will be used by the player to select
the ranges in which particular fast forward algorithms operate.

\param TrickModeParameters A pointer to a variable to hold the parameter structure.

\return Codec status code, CodecNoError indicates success.
*/

/*! \fn CodecStatus_t Codec_c::Connect(Port_c *Port)
\brief Connect to a Port_c on which decoded frames are to be placed.

\param Port Pointer to a Port_c instance.

\return Codec status code, CodecNoError indicates success.
*/

/*! \fn CodecStatus_t Codec_c::OutputPartialDecodeBuffers()
\brief Request for incompletely decoded buffers to be immediately output.

Passes onto the output ring any decode buffers that are partially
filled, this includes buffers with only one field decoded, or a number
of slices. In the event that several slices have been queued but not
decoded, they should be decoded and the relevant buffer passed on.

\return Codec status code, CodecNoError indicates success.
*/

/*! \fn CodecStatus_t Codec_c::DiscardQueuedDecodes()
\brief Discard any ongoing decodes.

This function is non-blocking.

\return Codec status code, CodecNoError indicates success.
*/

/*! \fn CodecStatus_t Codec_c::ReleaseReferenceFrame(unsigned int ReferenceFrameDecodeIndex)
\brief Release a reference frame.

<b>TODO: This method is not adequately documented.</b>

\param ReferenceFrameDecodeIndex Decode index of the frame to be released.

\return Codec status code, CodecNoError indicates success.
*/

/*! \fn CodecStatus_t Codec_c::CheckReferenceFrameList(
                        unsigned int          NumberOfReferenceFrameLists,
                        ReferenceFrameList_t      ReferenceFrameList[] )

\brief Check a reference frame list

This function checks that all the frames mentioned in the supplied lists are available
for use as reference frames. It is used, when frames are being discarded due to trick modes,
to ensure that a frame can be decoded.

\param NumberOfReferenceFrameLists, Number of reference frame lists (one for mpeg2 et al, two for H264)
\param ReferenceFrameList, the array of reference frame lists

\return Codec status code, CodecNoError indicates success.
*/

/*! \fn CodecStatus_t Codec_c::ReleaseDecodeBuffer(Buffer_t Buffer)
\brief Release a decode buffer.

<b>TODO: This method is not adequately documented.</b>

\param Buffer A pointer to the Buffer_c instance.

\return Codec status code, CodecNoError indicates success.
*/

/*! \fn CodecStatus_t Codec_c::Input(Buffer_t CodedBuffer)
\brief Accept coded data for decode.

The buffer provided to this function is the output from a frame parser.

\param CodedBuffer A pointer to a Buffer_c instance of a parsed coded frame buffer.

\return Codec status code, CodecNoError indicates success.
*/

#endif
