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
#ifndef CODER_BASE_H
#define CODER_BASE_H

#include "coder.h"

#define CODED_FRAME_BUFFER_PARTITION "vid-encoded"

//can't make any assumption on min compression ratio for video encoding: take max theorical size
#define CODED_MAX_WIDTH              1920
#define CODED_MAX_HEIGHT             1088
#define CODED_MAX_HEADER_SIZE         200 // in bytes
#define CODED_VIDEO_FRAME_MAX_SIZE   ((400*(CODED_MAX_WIDTH/16)*(CODED_MAX_HEIGHT/16))+CODED_MAX_HEADER_SIZE) // 400 x "Number of macroblocs" + margin for header
#define CODED_FRAME_MAX_SIZE         (CODED_VIDEO_FRAME_MAX_SIZE)

// Output error if running, else warning: should never occur in 'normal' operation
// This is to handle the case when we abort a blocking GetBuffer() call on encode stream termination
// TODO (sle): remove when correct EOS handling implemented in tunneled transcode
#define CODER_ERROR_RUNNING(fmt, args...)               \
{                                                       \
    if (TestComponentState(ComponentRunning))           \
        SE_ERROR(fmt, ##args);                          \
    else                                                \
        SE_WARNING("Not Running State: " fmt, ##args);  \
}
// Return error code if running, else no error: should never occur in 'normal' operation
// This is to handle the case when we abort a blocking GetBuffer() call on encode stream termination
// TODO (sle): remove when correct EOS handling implemented in tunneled transcode
#define CODER_STATUS_RUNNING(Status)    (TestComponentState(ComponentRunning) ? (CoderStatus_t)Status : (CoderStatus_t)CoderNoError)

class Coder_Base_c: public Coder_c
{
public:
    Coder_Base_c(void);
    ~Coder_Base_c(void);

    CoderStatus_t   Halt(void);

    CoderStatus_t   RegisterOutputBufferRing(Ring_t     Ring);

    CoderStatus_t   Input(Buffer_t  Buffer);

    CoderStatus_t   GetNewCodedBuffer(Buffer_t *Buffer, uint32_t Size);

    CoderStatus_t   GetBufferPoolDescriptor(Buffer_t    Buffer);

    CoderStatus_t   ManageMemoryProfile(void);

    CoderStatus_t   RegisterBufferManager(BufferManager_t   BufferManager);

    CoderStatus_t   InitializeCoder(void);

    CoderStatus_t   TerminateCoder(void);

    CoderStatus_t   GetControl(stm_se_ctrl_t    Control,
                               void            *Data);

    CoderStatus_t   SetControl(stm_se_ctrl_t    Control,
                               const void      *Data);

    CoderStatus_t   GetCompoundControl(stm_se_ctrl_t    Control,
                                       void            *Data);

    CoderStatus_t   SetCompoundControl(stm_se_ctrl_t    Control,
                                       const void      *Data);

    CoderStatus_t   SignalEvent(stm_se_encode_stream_event_t Event);

    virtual CoderStatus_t  LowPowerEnter(void);
    virtual CoderStatus_t  LowPowerExit(void);

protected:
    OS_Mutex_t                    Lock;

    Buffer_t                      CodedFrameBuffer;
    BufferType_t                  CodedFrameBufferType;

    BufferType_t                  OutputMetaDataBufferType;
    BufferType_t                  MetaDataSequenceNumberType;

    BufferType_t                  InputBufferType;
    BufferType_t                  InputMetaDataBufferType;

    Ring_t                        OutputRing;

    BufferManager_t               BufferManager;

    // Coded Frame Pool Attributes
    BufferPool_t                  CodedFrameBufferPool;
    allocator_device_t            CodedFrameMemoryDevice;
    void                          *CodedFrameMemory[3];
    unsigned int                  CodedMemorySize;
    unsigned int                  CodedFrameMaximumSize;
    unsigned int                  CodedMaxNbBuffers;
    char                          CodedMemoryPartitionName[ALLOCATOR_MAX_PARTITION_NAME_SIZE];

    // Discontinuity
    stm_se_discontinuity_t        CoderDiscontinuity;

    /// By providing an access function to add to the output ring, we can control its usage and statistics.
    CoderStatus_t               Output(Buffer_t   Buffer, bool   Marker = false);
    CoderStatus_t               SetFrameMemory(unsigned int FrameSize, unsigned int NbBuffers);
    CoderStatus_t               DetectBufferDiscontinuity(Buffer_t   Buffer);
    CoderStatus_t               GenerateBufferEOS(Buffer_t   Buffer);

private:
    DISALLOW_COPY_AND_ASSIGN(Coder_Base_c);
};

// ---------------------------------------------------------------------
//
// Documentation
//

/*! \class Coder_Base_c
\brief Base class implementation for the Coder classes.

Notes:<br>
 Owns BaseEncoderContext <br>
 Q: Collects metadata types from children and registers them?
*/

#endif /* CODER_BASE_H */
