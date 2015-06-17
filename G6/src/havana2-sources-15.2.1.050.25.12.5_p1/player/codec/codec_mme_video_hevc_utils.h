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

#ifndef H_CODEC_MME_VIDEO_HEVC_UTILS
#define H_CODEC_MME_VIDEO_HEVC_UTILS

#include "hevcppio.h"

typedef enum
{
    HevcActionNull = 0,
    HevcActionCallOutputPartialDecodeBuffers,
    HevcActionCallDiscardQueuedDecodes,
    HevcActionCallReleaseReferenceFrame,
    HevcActionPassOnFrame,
    HevcActionPassOnPreProcessedFrame,
    HevcActionTermination
} HevcFramesInPreprocessorChainAction_t;

typedef struct FramesInHevcPpChain_s
{
    bool                                  Used;
    bool                                  FakeCmd;
    HevcFramesInPreprocessorChainAction_t Action;
    Buffer_t                              CodedBuffer;
    Buffer_t                              PreProcessorBuffer[HEVC_NUM_INTERMEDIATE_BUFFERS];
    ParsedFrameParameters_t               *ParsedFrameParameters;
    unsigned int                          DecodeFrameIndex;
    struct hevcpp_ioctl_queue_t           QueInfo;
    struct hevcpp_ioctl_dequeue_t         DequeInfo;
} HevcFramesInPreprocessorChain_t;

/* To keep r/w operations thread-safe -> ppFrames_c deal with *all*
   HevcFramesInPreprocessorChain_t array interactions */
class ppFramesRing_c
{
public:
    explicit ppFramesRing_c(int maxSize);
    int FinalizeInit(void);
    ~ppFramesRing_c(void);

    bool isEmpty(void);
    bool noFakeTaskPending(void);
    bool CheckRefListDecodeIndex(int idx);

    int Insert(HevcFramesInPreprocessorChain_t *ppFrame);
    int InsertFakeEntry(int frameIdx, HevcFramesInPreprocessorChainAction_t action);
    int Extract(HevcFramesInPreprocessorChain_t *ppFrame);
    int waitForFakeTaskPending();
    void signalFakeTaskPending();
    int getSize() { return mSize; }

private:
    HevcFramesInPreprocessorChain_t *mData;
    HevcFramesInPreprocessorChain_t *mRData;
    HevcFramesInPreprocessorChain_t *mWData;
    int mSize;
    int mFakeTaskNb;
    int mMaxSize;
    OS_Mutex_t mLock;
    OS_Event_t mEv;
    OS_Event_t mEvFakeTask;

    DISALLOW_COPY_AND_ASSIGN(ppFramesRing_c);
};

#endif //H_CODEC_MME_VIDEO_HEVC_UTILS
