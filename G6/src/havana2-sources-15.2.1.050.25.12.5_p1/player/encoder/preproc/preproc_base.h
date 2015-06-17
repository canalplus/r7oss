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
#ifndef PREPROC_BASE_H
#define PREPROC_BASE_H

#include "preproc.h"

#define PREPROC_FRAME_BUFFER_PARTITION "vid-enc-scaled"  // currently shared audio & video

#define PREPROC_AUDIO_FRAME_MAX_SIZE   (4096*8*4)    // 4096 samples per block * 8 channels * 4 blocks
#define PREPROC_VIDEO_FRAME_MAX_SIZE   (1920*1088*2) // assume max 422 configuration
#define PREPROC_FRAME_MAX_SIZE         (max(PREPROC_AUDIO_FRAME_MAX_SIZE, PREPROC_VIDEO_FRAME_MAX_SIZE))

#define BUFFER_PREPROC_CONTEXT       "PreprocContext"
#define BUFFER_PREPROC_CONTEXT_TYPE  {BUFFER_PREPROC_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, 0}

// Output error if running, else warning: should never occur in 'normal' operation
// This is to handle the case when we abort a blocking GetBuffer() call on encode stream termination
// TODO (sle): remove when correct EOS handling implemented in tunneled transcode
#define PREPROC_ERROR_RUNNING(fmt, args...)             \
{                                                       \
    if (TestComponentState(ComponentRunning))           \
        SE_ERROR(fmt, ##args);                          \
    else                                                \
        SE_WARNING("Not Running State: " fmt, ##args);  \
}
// Return error code if running, else no error: should never occur in 'normal' operation
// This is to handle the case when we abort a blocking GetBuffer() call on encode stream termination
// TODO (sle): remove when correct EOS handling implemented in tunneled transcode
#define PREPROC_STATUS_RUNNING(Status)    (TestComponentState(ComponentRunning) ? (PreprocStatus_t)Status : (PreprocStatus_t)PreprocNoError)

typedef struct PreprocConfiguration_s
{
    unsigned int            PreprocContextCount;
    BufferDataDescriptor_t *PreprocContextDescriptor;

} PreprocConfiguration_t;


class Preproc_Base_c : public Preproc_c
{
public:
    Preproc_Base_c(void);
    ~Preproc_Base_c(void);

    PreprocStatus_t   Halt(void);

    PreprocStatus_t   ManageMemoryProfile(void);

    PreprocStatus_t   RegisterBufferManager(BufferManager_t       BufferManager);

    PreprocStatus_t   RegisterOutputBufferRing(Ring_t             Ring);

    PreprocStatus_t   OutputPartialPreprocBuffers(void);

    PreprocStatus_t   Input(Buffer_t          Buffer);

    PreprocStatus_t   AbortBlockingCalls(void);

    PreprocStatus_t   GetControl(stm_se_ctrl_t  Control,
                                 void          *Data);

    PreprocStatus_t   SetControl(stm_se_ctrl_t  Control,
                                 const void    *Data);

    PreprocStatus_t   GetCompoundControl(stm_se_ctrl_t  Control,
                                         void          *Data);

    PreprocStatus_t   SetCompoundControl(stm_se_ctrl_t  Control,
                                         const void    *Data);

    PreprocStatus_t   InjectDiscontinuity(stm_se_discontinuity_t    Discontinuity);

    virtual PreprocStatus_t  LowPowerEnter(void);
    virtual PreprocStatus_t  LowPowerExit(void);

    // audio specific: enum eAccAcMode not explicited to avoid header dep
    virtual void             GetChannelConfiguration(int64_t *AcMode) {}

protected:
    PreprocConfiguration_t        Configuration;

    Buffer_t                      PreprocFrameBuffer;
    BufferType_t                  PreprocFrameBufferType;
    BufferType_t                  PreprocFrameAllocType;

    BufferType_t                  InputBufferType;
    BufferType_t                  InputMetaDataBufferType;
    BufferType_t                  EncodeCoordinatorMetaDataBufferType;
    BufferType_t                  OutputMetaDataBufferType;

    BufferType_t                  MetaDataSequenceNumberType;

    Ring_t                        OutputRing;

    BufferManager_t               BufferManager;

    // Preproc Frame Pool Attributes
    BufferPool_t                  PreprocFrameBufferPool;
    BufferPool_t                  PreprocFrameAllocPool;
    allocator_device_t            PreprocFrameMemoryDevice;
    void                         *PreprocFrameMemory[3];
    unsigned int                  PreprocMemorySize;
    unsigned int                  PreprocFrameMaximumSize;
    unsigned int                  PreprocMaxNbAllocBuffers;
    unsigned int                  PreprocMaxNbBuffers;
    char                          PreprocMemoryPartitionName[ALLOCATOR_MAX_PARTITION_NAME_SIZE];

    // Input Metadata
    // - uncompressed metadata: describes the uncompressed frame parameters
    // - encode coordinator metadata: In NRT mode, it contains additional info filled by the EncodeCoordinator in NRT mode.
    //   Otherwise it is not initialized, but will be set by the PreprocBase_c::Input() method for further use by the preprocessor.
    stm_se_uncompressed_frame_metadata_t *InputMetaDataDescriptor;
    __stm_se_encode_coordinator_metadata_t *EncodeCoordinatorMetaDataDescriptor;

    // Output Metadata
    // Internal metadata of type __stm_se_frame_metadata_t is filled by the preproc and attached to the output buffer.
    // We store locally the uncompress metadata subset pointer in order to simplify the preproc code.
    // The encode coordinator metadata subset struct will also be filled by the preproc with relevant data, whatever the selected NRT control mode.
    stm_se_uncompressed_frame_metadata_t *PreprocMetaDataDescriptor;
    __stm_se_frame_metadata_t            *PreprocFullMetaDataDescriptor;

    // Context
    Buffer_t                      PreprocContextBuffer;
    BufferType_t                  PreprocContextBufferType;
    BufferPool_t                  PreprocContextBufferPool;

    // Discontinuity
    stm_se_discontinuity_t        PreprocDiscontinuity;
    uint32_t                      InputBufferSize;

    // By providing an access function to add to the output ring, we can control its usage and statistics.
    PreprocStatus_t               Output(Buffer_t   Buffer, bool   Marker = false);
    PreprocStatus_t               GetNewBuffer(Buffer_t   *Buffer, bool IsADiscontinuityBuffer = false);
    PreprocStatus_t               GetBufferClone(Buffer_t   Buffer, Buffer_t *CloneBuffer);
    PreprocStatus_t               GetNewContext(Buffer_t   *Buffer);
    PreprocStatus_t               SetFrameMemory(unsigned int FrameSize, unsigned int NbBuffers);
    PreprocStatus_t               CheckDiscontinuity(stm_se_discontinuity_t   Discontinuity);
    PreprocStatus_t               DetectBufferDiscontinuity(Buffer_t   Buffer);
    PreprocStatus_t               GenerateBufferDiscontinuity(stm_se_discontinuity_t Discontinuity);
    PreprocStatus_t               CheckNativeTimeFormat(stm_se_time_format_t NativeTimeFormat, uint64_t NativeTime);
    void                          DumpInputMetadata(stm_se_uncompressed_frame_metadata_t *Metadata);
    void                          DumpEncodeCoordinatorMetadata(__stm_se_encode_coordinator_metadata_t *Metadata);

private:
    DISALLOW_COPY_AND_ASSIGN(Preproc_Base_c);
};

// ---------------------------------------------------------------------
//
// Documentation
//

/*! \class Preproc_Base_c
\brief Base class implementation for the Preprocessor classes.

TODO: Q: Should the preproc avoid a copy of the input buffers in the case where there is no preprocessing to be done?
*/

#endif /* PREPROC_BASE_H */
