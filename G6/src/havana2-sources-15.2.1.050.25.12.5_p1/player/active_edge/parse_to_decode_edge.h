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
#ifndef H_PARSE_TO_DECODE
#define H_PARSE_TO_DECODE

#include "active_edge_base.h"

typedef struct DecoderConfig_s
{
    // Mutex to Lock the Update field that can be accessed
    // in the context of the application and of the ParseToDecode thread
    OS_Mutex_t   Lock;

    // BitField reporting which control has been updated.
    unsigned int Update;
    unsigned int Applied;
} DecoderConfig_t;



class ParseToDecodeEdge_c : public ActiveEdge_Base_c
{
public:
    ParseToDecodeEdge_c(int ThreadDescId, PlayerStream_t Stream, int InputRingSize, Codec_t Codec)
        : ActiveEdge_Base_c(ThreadDescId, Stream, InputRingSize)
        , mMaximumActualSequenceNumberSeen(0)
        , mSequenceNumber(INVALID_SEQUENCE_VALUE)
        , mPromoteNextStreamParametersToNew(false)
        , mCodec(Codec)
        , mDecoderConfig()
    {
        //
        // Manage Config parameters
        //
        OS_InitializeMutex(&mDecoderConfig.Lock);
    }

    ~ParseToDecodeEdge_c(void)
    {
        OS_TerminateMutex(&mDecoderConfig.Lock);
    }

    void   RecordNonDecodedFrame(Buffer_t Buffer, ParsedFrameParameters_t  *ParsedFrameParameters);

    virtual PlayerStatus_t   CallInSequence(PlayerSequenceType_t      SequenceType,
                                            PlayerSequenceValue_t     SequenceValue,
                                            PlayerComponentFunction_t Fn,
                                            ...);

    Codec_t GetCodec() { return mCodec; }

    void    ApplyDecoderConfig(unsigned int Config)
    {
        OS_LockMutex(&mDecoderConfig.Lock);
        mDecoderConfig.Update  |= Config; // will be reset by Codec once Applied
        mDecoderConfig.Applied |= Config; // is static for the life-time of the stream
        OS_UnLockMutex(&mDecoderConfig.Lock);
    }

    void    ReapplyDecoderConfig(void)
    {
        OS_LockMutex(&mDecoderConfig.Lock);
        mDecoderConfig.Update  |= mDecoderConfig.Applied;
        OS_UnLockMutex(&mDecoderConfig.Lock);
    }

    unsigned int CheckDecoderConfig(void)
    {
        return mDecoderConfig.Update;
    }

    unsigned int GetDecoderConfig(void)
    {
        OS_LockMutex(&mDecoderConfig.Lock);
        int update = mDecoderConfig.Update;
        mDecoderConfig.Update = 0;
        OS_UnLockMutex(&mDecoderConfig.Lock);
        return update;
    }

private:
    unsigned long long      mMaximumActualSequenceNumberSeen;
    unsigned long long      mSequenceNumber;
    bool                    mPromoteNextStreamParametersToNew;
    Codec_t                 mCodec;

    // Current decoding parameters
    DecoderConfig_t         mDecoderConfig;

    virtual void MainLoop();
    virtual PlayerStatus_t  PerformInSequenceCall(PlayerControlStructure_t *ControlStructure);

    void SwitchCodec();

    void UpdateCodecErrorStatistics(PlayerStatus_t Status);
    bool TestDiscardBuffer(Buffer_t Buffer, ParsedFrameParameters_t *ParsedFrameParameters);
    void HandleMarkerFrame(Buffer_t Buffer, PlayerSequenceNumber_t *SequenceNumberStructure, ParsedFrameParameters_t *ParsedFrameParameters);
    void HandleCodedFrameBuffer(Buffer_t Buffer, PlayerSequenceNumber_t *SequenceNumberStructure, ParsedFrameParameters_t *ParsedFrameParameters);
    void HandleCodedFrameBufferType(Buffer_t Buffer);

    DISALLOW_COPY_AND_ASSIGN(ParseToDecodeEdge_c);
};

#endif // H_PARSE_TO_DECODE
