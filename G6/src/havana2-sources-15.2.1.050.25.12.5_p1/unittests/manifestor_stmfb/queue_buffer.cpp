#include "manifestor_stmfb_ctest.h"

using ::testing::Invoke;
using ::testing::InSequence;

MATCHER_P(IsProgressive, count, "") { return arg->info.nfields == count && (arg->src.flags & STM_BUFFER_SRC_INTERLACED) == 0; }
MATCHER_P(IsTopField, count, "") { return arg->info.nfields == count && (arg->src.flags & STM_BUFFER_SRC_INTERLACED) && (arg->src.flags & STM_BUFFER_SRC_TOP_FIELD_ONLY); }
MATCHER_P(IsBottomField, count, "") { return arg->info.nfields == count && (arg->src.flags & STM_BUFFER_SRC_INTERLACED) && (arg->src.flags & STM_BUFFER_SRC_BOTTOM_FIELD_ONLY); }

#define MAX_QUEUED_BUFFERS 10
stm_display_buffer_t gQueuedBuffers[MAX_QUEUED_BUFFERS];
int gQueuedBufferIndex = 0;

void StoreQueuedBuffer(stm_display_source_queue_h queue, const stm_display_buffer_t *pBuffer) {
    ASSERT_GT(MAX_QUEUED_BUFFERS, gQueuedBufferIndex);
    gQueuedBuffers[gQueuedBufferIndex++] = *pBuffer;
}

void ReturnBuffers() {
    for (int i = 0; i < gQueuedBufferIndex; i++) {

        stm_display_buffer_t *pBuffer = &gQueuedBuffers[i];

        if (pBuffer->info.display_callback) {
            stm_display_latency_params_t params;
            params.output_id = 1;
            params.plane_id = 1;
            params.output_latency_in_us = 100000;
            params.output_change = 0;
            pBuffer->info.display_callback(pBuffer->info.puser_data, 0, 0, 1, &params);
        }
        if (pBuffer->info.completed_callback) {
            pBuffer->info.completed_callback(pBuffer->info.puser_data, 0);
        }
    }
}

TEST_F(Manifestor_stmfb_cTest, ProgressiveContent) {
    MockBuffer_c mockBuffer1(mMockPlayer), mockBuffer2(mMockPlayer), mockBuffer3(mMockPlayer);
    MockBuffer_c mockCodedBuffer1(mMockPlayer), mockCodedBuffer2(mMockPlayer), mockCodedBuffer3(mMockPlayer);

    mockBuffer1.AttachBufferReference(&mockCodedBuffer1);
    mockBuffer2.AttachBufferReference(&mockCodedBuffer2);
    mockBuffer3.AttachBufferReference(&mockCodedBuffer3);

    EXPECT_CALL(mMockVibe, stm_display_source_queue_buffer(_, IsProgressive(1)))
    .WillOnce(DoAll(
                  Invoke(StoreQueuedBuffer),
                  Return(0)));
    EXPECT_CALL(mMockVibe, stm_display_source_queue_buffer(_, IsProgressive(4)))
    .WillOnce(DoAll(
                  Invoke(StoreQueuedBuffer),
                  Return(0)));
    EXPECT_CALL(mMockOutputPort, InsertFromIRQ(_))
    .Times(2)
    .WillRepeatedly(Return(RingNoError));

    mockBuffer1.mVideoParams.InterlacedFrame = false;
    mockBuffer2.mVideoParams.InterlacedFrame = false;
    mockBuffer3.mVideoParams.InterlacedFrame = false;

    mManifestorStmfb.UpdateOutputSurfaceDescriptor();

    mSourceTiming.Interlaced        = false;
    mSourceTiming.DisplayCount[0]   = 1;
    mSourceTiming.DisplayCount[1]   = 0;

    unsigned int numTimings = mNumOutputs + 1;
    mManifestorStmfb.QueueDecodeBuffer(&mockBuffer2, mTimings, &numTimings);

    mManifestorStmfb.UpdateOutputSurfaceDescriptor();

    mSourceTiming.DisplayCount[0]   = 4;
    mManifestorStmfb.QueueDecodeBuffer(&mockBuffer3, mTimings, &numTimings);

    ReturnBuffers();
}

TEST_F(Manifestor_stmfb_cTest, InterlacedContent) {

    MockBuffer_c mockBuffer1(mMockPlayer), mockBuffer2(mMockPlayer), mockBuffer3(mMockPlayer);
    MockBuffer_c mockCodedBuffer1(mMockPlayer), mockCodedBuffer2(mMockPlayer), mockCodedBuffer3(mMockPlayer);

    mockBuffer1.AttachBufferReference(&mockCodedBuffer1);
    mockBuffer2.AttachBufferReference(&mockCodedBuffer2);
    mockBuffer3.AttachBufferReference(&mockCodedBuffer3);

    {
        InSequence s;

        EXPECT_CALL(mMockVibe, stm_display_source_queue_buffer(_, IsTopField(1)))
        .WillOnce(DoAll(
                      Invoke(StoreQueuedBuffer),
                      Return(0)));
        EXPECT_CALL(mMockVibe, stm_display_source_queue_buffer(_, IsBottomField(1)))
        .WillOnce(DoAll(
                      Invoke(StoreQueuedBuffer),
                      Return(0)));

        EXPECT_CALL(mMockVibe, stm_display_source_queue_buffer(_, IsTopField(3)))
        .WillOnce(DoAll(
                      Invoke(StoreQueuedBuffer),
                      Return(0)));
        EXPECT_CALL(mMockVibe, stm_display_source_queue_buffer(_, IsBottomField(1)))
        .WillOnce(DoAll(
                      Invoke(StoreQueuedBuffer),
                      Return(0)));
    }

    EXPECT_CALL(mMockOutputPort, InsertFromIRQ(_))
    .Times(2)
    .WillRepeatedly(Return(RingNoError));

    mockBuffer1.mVideoParams.InterlacedFrame = true;
    mockBuffer2.mVideoParams.InterlacedFrame = true;
    mockBuffer3.mVideoParams.InterlacedFrame = true;

    mSourceTiming.Interlaced        = true;
    mSourceTiming.TopFieldFirst     = true;
    mSourceTiming.DisplayCount[0]   = 1;
    mSourceTiming.DisplayCount[1]   = 1;

    mManifestorStmfb.UpdateOutputSurfaceDescriptor();
    unsigned int numTimings = mNumOutputs + 1;
    mManifestorStmfb.QueueDecodeBuffer(&mockBuffer1, mTimings, &numTimings);

    mSourceTiming.DisplayCount[0]   = 3;
    mSourceTiming.DisplayCount[1]   = 1;

    mManifestorStmfb.UpdateOutputSurfaceDescriptor();
    mManifestorStmfb.QueueDecodeBuffer(&mockBuffer2, mTimings, &numTimings);

    ReturnBuffers();
}
