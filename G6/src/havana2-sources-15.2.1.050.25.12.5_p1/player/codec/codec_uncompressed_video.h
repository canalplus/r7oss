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

#ifndef H_CODEC_UNCOMPRESSED_VIDEO
#define H_CODEC_UNCOMPRESSED_VIDEO

#include "codec.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_UncompressedVideo_c"

class Codec_UncompressedVideo_c : public Codec_c
{
public:
    Codec_UncompressedVideo_c(void);
    ~Codec_UncompressedVideo_c(void);

    //
    // Standard class functions
    //

    CodecStatus_t   GetTrickModeParameters(CodecTrickModeParameters_t      *TrickModeParameters);

    CodecStatus_t   Connect(Port_c *Port);

    CodecStatus_t   ReleaseDecodeBuffer(Buffer_t                  Buffer);

    CodecStatus_t   Input(Buffer_t                  CodedBuffer);

    //
    // Stubbed out functions that do nothing in an uncompressed situation
    //

    void            UpdateConfig(unsigned int Config) { }

    CodecStatus_t   OutputPartialDecodeBuffers(void)        {return CodecNoError;}
    CodecStatus_t   DiscardQueuedDecodes(void)      {return CodecNoError;}
    CodecStatus_t   ReleaseReferenceFrame(unsigned int ReferenceFrameDecodeIndex)       {return CodecNoError;}
    CodecStatus_t   CheckReferenceFrameList(unsigned int NumberOfReferenceFrameLists,
                                            ReferenceFrameList_t    ReferenceFrameList[])       {return CodecNoError;}

    // Stubbed low power methods (nothing to do for uncompressed)
    CodecStatus_t   LowPowerEnter(void)                {return CodecNoError;}
    CodecStatus_t   LowPowerExit(void)                 {return CodecNoError;}

    //
    // Stubbed out base class function that wrapper expects me to have
    //

    CodecStatus_t   SetModuleParameters(unsigned int ParameterBlockSize, void *ParameterBlock)       {return CodecNoError;}

    CodecStatus_t   UpdatePlaybackSpeed(void)
    {
        SE_WARNING("Trickmode feature not available for this codec\n");
        return CodecNoError;
    }

protected:
    BufferManager_t                   BufferManager;
    bool                              DataTypesInitialized;

    CodecTrickModeParameters_t        UncompressedTrickModeParameters;

    BufferPool_t                      DecodeBufferPool;

    Port_c                           *mOutputPort;

private:
    DISALLOW_COPY_AND_ASSIGN(Codec_UncompressedVideo_c);
};

#endif // H_CODEC_UNCOMPRESSED_VIDEO
