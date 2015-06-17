#include "es_processor_ctest.h"
#include "mock_buffer.h"

using ::testing::InSequence;

TEST_F(ES_Processor_cTest, InitialState) {
    MockBuffer_c buffer1(mMockPlayer), buffer2(mMockPlayer), buffer3(mMockPlayer);

    // mMockPort expectations
    EXPECT_CALL(mMockPort, Insert((uintptr_t) &buffer1))
    .WillOnce(Return(RingNoError));
    EXPECT_CALL(mMockPort, Insert((uintptr_t) &buffer2))
    .WillOnce(Return(RingNoError));
    EXPECT_CALL(mMockPort, Insert((uintptr_t) &buffer3))
    .WillOnce(Return(RingNoError));

    EXPECT_CALL(mMockPort, NonEmpty())
    .WillOnce(Return(true))
    .WillOnce(Return(false));

    // Test that every buffer pushed to ES Processor is pushed to the connected input port
    buffer1.mCodedFrameParams.PlaybackTime = 80000;
    buffer1.mCodedFrameParams.PlaybackTimeValid = true;
    RingStatus_t status = mESProcessor.Insert((uintptr_t) &buffer1);
    EXPECT_EQ(RingNoError, status);

    buffer2.mCodedFrameParams.PlaybackTime = 90000;
    buffer2.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &buffer2);
    EXPECT_EQ(RingNoError, status);

    buffer3.mCodedFrameParams.PlaybackTime = 100000;
    buffer3.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &buffer3);
    EXPECT_EQ(RingNoError, status);

    // Test that NonEmpty method returns the value returned by the downstream Port
    bool nonEmpty = mESProcessor.NonEmpty();
    EXPECT_EQ(true, nonEmpty);

    nonEmpty = mESProcessor.NonEmpty();
    EXPECT_EQ(false, nonEmpty);
}

MATCHER_P(IsPTSDiscardingEvent, isStart, "") { return (arg->Code == EventDiscarding) && (arg->Value[0].UnsignedInt == STM_SE_PLAY_STREAM_PTS_TRIGGER) && (arg->Value[1].Bool == isStart); }

MATCHER_P(IsSplicingMarkerDiscardingEvent, isStart, "") { return (arg->Code == EventDiscarding) && (arg->Value[0].UnsignedInt == STM_SE_PLAY_STREAM_SPLICING_MARKER_TRIGGER) && (arg->Value[1].Bool == isStart); }

MATCHER_P2(IsCorrectBuffer, Expectedbuffer, ExpectedPlaybackTime, "") {
    MockBuffer_c *Buffer = (MockBuffer_c *) arg;
    return (Buffer == Expectedbuffer) && (Buffer->mCodedFrameParams.PlaybackTime == ExpectedPlaybackTime);
}

TEST_F(ES_Processor_cTest, LiveToDiskTransition) {
    MockBuffer_c buffer1(mMockPlayer), buffer2(mMockPlayer), buffer3(mMockPlayer); // buffer that should be forwarded by ES Processor
    MockBuffer_c discarded1(mMockPlayer), discarded2(mMockPlayer); // buffer that should be discarded by ES Processor
    MockBuffer_c markerFrame(mMockPlayer);

    {
        InSequence s;

        // mMockPort expectations
        EXPECT_CALL(mMockPort, Insert(IsCorrectBuffer(&buffer1, 80000)))
        .WillOnce(Return(RingNoError));
        EXPECT_CALL(mMockPort, Insert(IsCorrectBuffer(&buffer2, 100000)))
        .WillOnce(Return(RingNoError));
        EXPECT_CALL(mMockStream, SignalEvent(IsPTSDiscardingEvent(true)))
        .WillOnce(Return(PlayerNoError));
        EXPECT_CALL(mMockStream, SignalEvent(IsSplicingMarkerDiscardingEvent(false)))
        .WillOnce(Return(PlayerNoError));
        EXPECT_CALL(mMockPort, Insert(IsCorrectBuffer(&buffer3, 160000)))
        .WillOnce(Return(RingNoError));
    }

    // Set a start trigger on PTS
    stm_se_play_stream_discard_trigger_t start_trigger;
    start_trigger.type          = STM_SE_PLAY_STREAM_PTS_TRIGGER;
    start_trigger.start_not_end = true;
    start_trigger.u.pts_trigger.pts = 120000;
    start_trigger.u.pts_trigger.tolerance = 0;

    PlayerStatus_t status = mESProcessor.SetDiscardTrigger(start_trigger);
    EXPECT_EQ(PlayerNoError, status);

    // ES Processor should now be in StartDiscardSet state
    // buffers pushed should be forwarded to the connected input port
    buffer1.mCodedFrameParams.PlaybackTime = 80000;
    buffer1.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &buffer1);
    EXPECT_EQ(RingNoError, status);

    // Set end trigger on splicing discontinuity
    stm_se_play_stream_discard_trigger_t end_trigger;
    end_trigger.type          = STM_SE_PLAY_STREAM_SPLICING_MARKER_TRIGGER;
    end_trigger.start_not_end = false;

    status = mESProcessor.SetDiscardTrigger(end_trigger);
    EXPECT_EQ(PlayerNoError, status);

    // ES Processor should now be in StartEndDiscardSet state
    // buffers pushed should be forwarded to the connected input port until
    // discard PTS is received
    buffer2.mCodedFrameParams.PlaybackTime = 100000;
    buffer2.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &buffer2);
    EXPECT_EQ(RingNoError, status);

    // Now push a buffer with the right PTS
    // The ES processor should now discard any buffer pushed until
    // marker frame is received
    discarded1.mCodedFrameParams.PlaybackTime = start_trigger.u.pts_trigger.pts;
    discarded1.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &discarded1);
    EXPECT_EQ(RingNoError, status);

    discarded2.mCodedFrameParams.PlaybackTime = 140000;
    discarded2.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &discarded2);
    EXPECT_EQ(RingNoError, status);

    // Push a splicing discontinuity marker frame
    markerFrame.mSequenceNumber.MarkerFrame = true;
    markerFrame.mSequenceNumber.MarkerType  = SplicingMarker;
    markerFrame.mSequenceNumber.SplicingMarkerData.PTS_offset = 0;
    status = mESProcessor.Insert((uintptr_t) &markerFrame);
    EXPECT_EQ(RingNoError, status);

    // Discarding is over and buffer should flow
    buffer3.mCodedFrameParams.PlaybackTime = 160000;
    buffer3.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &buffer3);
    EXPECT_EQ(RingNoError, status);
}

TEST_F(ES_Processor_cTest, DiskToLiveTransition) {
    MockBuffer_c buffer1(mMockPlayer), buffer2(mMockPlayer), buffer3(mMockPlayer); // buffer that should be forwarded by ES Processor
    MockBuffer_c discarded1(mMockPlayer), discarded2(mMockPlayer); // buffer that should be discarded by ES Processor
    MockBuffer_c markerFrame(mMockPlayer);

    {
        InSequence s;

        // mMockPort expectations
        EXPECT_CALL(mMockPort, Insert(IsCorrectBuffer(&buffer1, 80000)))
        .WillOnce(Return(RingNoError));
        EXPECT_CALL(mMockPort, Insert(IsCorrectBuffer(&buffer2, 100000)))
        .WillOnce(Return(RingNoError));
        EXPECT_CALL(mMockStream, SignalEvent(IsSplicingMarkerDiscardingEvent(true)))
        .WillOnce(Return(PlayerNoError));
        EXPECT_CALL(mMockStream, SignalEvent(IsPTSDiscardingEvent(false)))
        .WillOnce(Return(PlayerNoError));
        EXPECT_CALL(mMockPort, Insert(IsCorrectBuffer(&buffer3, 160000)))
        .WillOnce(Return(RingNoError));
    }

    // Set start trigger on splicing discontinuity
    stm_se_play_stream_discard_trigger_t start_trigger;
    start_trigger.type          = STM_SE_PLAY_STREAM_SPLICING_MARKER_TRIGGER;
    start_trigger.start_not_end = true;

    PlayerStatus_t status = mESProcessor.SetDiscardTrigger(start_trigger);
    EXPECT_EQ(PlayerNoError, status);

    // ES Processor should now be in StartDiscardSet state
    // buffers pushed should be forwarded to the connected input port
    buffer1.mCodedFrameParams.PlaybackTime = 80000;
    buffer1.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &buffer1);
    EXPECT_EQ(RingNoError, status);

    // Set a end trigger on PTS
    stm_se_play_stream_discard_trigger_t end_trigger;
    end_trigger.type          = STM_SE_PLAY_STREAM_PTS_TRIGGER;
    end_trigger.start_not_end = false;
    end_trigger.u.pts_trigger.pts = 160000;
    end_trigger.u.pts_trigger.tolerance = 0;

    status = mESProcessor.SetDiscardTrigger(end_trigger);
    EXPECT_EQ(PlayerNoError, status);

    // ES Processor should now be in StartEndDiscardSet state
    // buffers pushed should be forwarded to the connected input port until
    // marker frame is received
    buffer2.mCodedFrameParams.PlaybackTime = 100000;
    buffer2.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &buffer2);
    EXPECT_EQ(RingNoError, status);

    // Push a splicing discontinuity marker frame
    markerFrame.mSequenceNumber.MarkerFrame = true;
    markerFrame.mSequenceNumber.MarkerType  = SplicingMarker;
    markerFrame.mSequenceNumber.SplicingMarkerData.PTS_offset = 0;
    status = mESProcessor.Insert((uintptr_t) &markerFrame);
    EXPECT_EQ(RingNoError, status);

    // Now push a buffer with the right PTS
    // The ES processor should now discard any buffer pushed until
    // end PTS Trigger is received
    discarded1.mCodedFrameParams.PlaybackTime = 120000;
    discarded1.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &discarded1);
    EXPECT_EQ(RingNoError, status);

    discarded2.mCodedFrameParams.PlaybackTime = 140000;
    discarded2.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &discarded2);
    EXPECT_EQ(RingNoError, status);

    // Discarding is over and buffer should flow
    buffer3.mCodedFrameParams.PlaybackTime = 160000;
    buffer3.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &buffer3);
    EXPECT_EQ(RingNoError, status);
}

MATCHER(IsAlarmPtsEvent, "") { return arg->Code == EventAlarmPts; }

TEST_F(ES_Processor_cTest, PtsAlarmNotification) {
    MockBuffer_c buffer1(mMockPlayer), buffer2(mMockPlayer), buffer3(mMockPlayer);

    {
        InSequence s;

        // mMockPort expectations
        EXPECT_CALL(mMockPort, Insert(IsCorrectBuffer(&buffer1, 80000)))
        .WillOnce(Return(RingNoError));
        EXPECT_CALL(mMockPort, Insert(IsCorrectBuffer(&buffer2, 100000)))
        .WillOnce(Return(RingNoError));
        EXPECT_CALL(mMockStream, SignalEvent(IsAlarmPtsEvent()))
        .WillOnce(Return(PlayerNoError));
        EXPECT_CALL(mMockPort, Insert(IsCorrectBuffer(&buffer3, 120000)))
        .WillOnce(Return(RingNoError));
    }

    // Set a PTS Alarm
    stm_se_play_stream_pts_and_tolerance_t alarm;
    alarm.pts = 120000;
    alarm.tolerance = 0;

    PlayerStatus_t status = mESProcessor.SetAlarm(true, alarm);
    EXPECT_EQ(PlayerNoError, status);

    buffer1.mCodedFrameParams.PlaybackTime = 80000;
    buffer1.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &buffer1);
    EXPECT_EQ(RingNoError, status);

    buffer2.mCodedFrameParams.PlaybackTime = 100000;
    buffer2.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &buffer2);
    EXPECT_EQ(RingNoError, status);

    // Alarm should be detected on this buffer
    buffer3.mCodedFrameParams.PlaybackTime = 120000;
    buffer3.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &buffer3);
    EXPECT_EQ(RingNoError, status);

    // End alarm
    status = mESProcessor.SetAlarm(false, alarm);
    EXPECT_EQ(PlayerNoError, status);
}

TEST_F(ES_Processor_cTest, CancelPtsAlarmNotification) {
    MockBuffer_c buffer1(mMockPlayer), buffer2(mMockPlayer), buffer3(mMockPlayer), buffer4(mMockPlayer);

    {
        InSequence s;

        // mMockPort expectations
        EXPECT_CALL(mMockPort, Insert(IsCorrectBuffer(&buffer1, 80000)))
        .WillOnce(Return(RingNoError));
        EXPECT_CALL(mMockPort, Insert(IsCorrectBuffer(&buffer2, 100000)))
        .WillOnce(Return(RingNoError));
        EXPECT_CALL(mMockPort, Insert(IsCorrectBuffer(&buffer3, 120000)))
        .WillOnce(Return(RingNoError));
        EXPECT_CALL(mMockPort, Insert(IsCorrectBuffer(&buffer4, 140000)))
        .WillOnce(Return(RingNoError));
    }

    // Set a PTS Alarm
    stm_se_play_stream_pts_and_tolerance_t alarm;
    alarm.pts = 100000;
    alarm.tolerance = 0;

    PlayerStatus_t status = mESProcessor.SetAlarm(true, alarm);
    EXPECT_EQ(PlayerNoError, status);

    buffer1.mCodedFrameParams.PlaybackTime = 80000;
    buffer1.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &buffer1);
    EXPECT_EQ(RingNoError, status);

    // Cancel PTS alarm
    status = mESProcessor.SetAlarm(false, alarm);
    EXPECT_EQ(PlayerNoError, status);

    buffer2.mCodedFrameParams.PlaybackTime = 100000;
    buffer2.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &buffer2);
    EXPECT_EQ(RingNoError, status);

    // Set a PTS Alarm
    alarm.pts = 120001;
    alarm.tolerance = 20000;

    status = mESProcessor.SetAlarm(true, alarm);
    EXPECT_EQ(PlayerNoError, status);

    buffer3.mCodedFrameParams.PlaybackTime = 120000;
    buffer3.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &buffer3);
    EXPECT_EQ(RingNoError, status);

    // Reset should cancel the Alarm
    status = mESProcessor.Reset();
    EXPECT_EQ(PlayerNoError, status);

    buffer4.mCodedFrameParams.PlaybackTime = 140000;
    buffer4.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &buffer4);
    EXPECT_EQ(RingNoError, status);
}

TEST_F(ES_Processor_cTest, StartLiveFromLocalAds) {
    MockBuffer_c buffer1(mMockPlayer), buffer2(mMockPlayer), buffer3(mMockPlayer),
                 buffer4(mMockPlayer), buffer5(mMockPlayer), buffer6(mMockPlayer);
    MockBuffer_c markerFrame(mMockPlayer);

    {
        InSequence s;

        // mMockPort expectations
        EXPECT_CALL(mMockPort, Insert(IsCorrectBuffer(&buffer1, 80000)))
        .WillOnce(Return(RingNoError));
        EXPECT_CALL(mMockPort, Insert(IsCorrectBuffer(&buffer2, 100000)))
        .WillOnce(Return(RingNoError));
        EXPECT_CALL(mMockPort, Insert(IsCorrectBuffer(&buffer3, 120000)))
        .WillOnce(Return(RingNoError));
        EXPECT_CALL(mMockPort, Insert(IsCorrectBuffer(&buffer4, 140000)))
        .WillOnce(Return(RingNoError));
        EXPECT_CALL(mMockPort, Insert(IsCorrectBuffer(&buffer5, 160000)))
        .WillOnce(Return(RingNoError));
        EXPECT_CALL(mMockPort, Insert(IsCorrectBuffer(&buffer6, 180000)))
        .WillOnce(Return(RingNoError));
    }

    buffer1.mCodedFrameParams.PlaybackTime = 80000;
    buffer1.mCodedFrameParams.PlaybackTimeValid = true;
    PlayerStatus_t status = mESProcessor.Insert((uintptr_t) &buffer1);
    EXPECT_EQ(RingNoError, status);

    // Push a splicing discontinuity marker frame
    markerFrame.mSequenceNumber.MarkerFrame = true;
    markerFrame.mSequenceNumber.MarkerType  = SplicingMarker;
    markerFrame.mSequenceNumber.SplicingMarkerData.PTS_offset = 5000;
    status = mESProcessor.Insert((uintptr_t) &markerFrame);
    EXPECT_EQ(RingNoError, status);

    // Start injection of first Ad
    buffer2.mCodedFrameParams.PlaybackTime = 95000;
    buffer2.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &buffer2);
    EXPECT_EQ(RingNoError, status);

    buffer3.mCodedFrameParams.PlaybackTime = 115000;
    buffer3.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &buffer3);
    EXPECT_EQ(RingNoError, status);

    // Push a splicing discontinuity marker frame
    markerFrame.mSequenceNumber.MarkerFrame = true;
    markerFrame.mSequenceNumber.MarkerType  = SplicingMarker;
    markerFrame.mSequenceNumber.SplicingMarkerData.PTS_offset = -15000;
    status = mESProcessor.Insert((uintptr_t) &markerFrame);
    EXPECT_EQ(RingNoError, status);

    // Start injection of 2nd Ad
    buffer4.mCodedFrameParams.PlaybackTime = 150000;
    buffer4.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &buffer4);
    EXPECT_EQ(RingNoError, status);

    buffer5.mCodedFrameParams.PlaybackTime = 170000;
    buffer5.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &buffer5);
    EXPECT_EQ(RingNoError, status);

    // Push a splicing discontinuity marker frame
    markerFrame.mSequenceNumber.MarkerFrame = true;
    markerFrame.mSequenceNumber.MarkerType  = SplicingMarker;
    markerFrame.mSequenceNumber.SplicingMarkerData.PTS_offset = 10000;
    status = mESProcessor.Insert((uintptr_t) &markerFrame);
    EXPECT_EQ(RingNoError, status);

    // Back to live
    buffer6.mCodedFrameParams.PlaybackTime = 180000;
    buffer6.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &buffer6);
    EXPECT_EQ(RingNoError, status);
}

TEST_F(ES_Processor_cTest, CancelDiskToLiveTransition) {
    MockBuffer_c buffer1(mMockPlayer), buffer2(mMockPlayer), buffer3(mMockPlayer); // buffer that should be forwarded by ES Processor
    MockBuffer_c discarded1(mMockPlayer), discarded2(mMockPlayer); // buffer that should be discarded by ES Processor
    MockBuffer_c markerFrame(mMockPlayer);

    {
        InSequence s;

        // mMockPort expectations
        EXPECT_CALL(mMockPort, Insert(IsCorrectBuffer(&buffer1, 80000)))
        .WillOnce(Return(RingNoError));
        EXPECT_CALL(mMockPort, Insert(IsCorrectBuffer(&buffer2, 100000)))
        .WillOnce(Return(RingNoError));
        EXPECT_CALL(mMockStream, SignalEvent(IsSplicingMarkerDiscardingEvent(true)))
        .WillOnce(Return(PlayerNoError));
        EXPECT_CALL(mMockPort, Insert(IsCorrectBuffer(&buffer3, 140000)))
        .WillOnce(Return(RingNoError));
    }

    // Set start trigger on splicing discontinuity
    stm_se_play_stream_discard_trigger_t start_trigger;
    start_trigger.type          = STM_SE_PLAY_STREAM_SPLICING_MARKER_TRIGGER;
    start_trigger.start_not_end = true;

    PlayerStatus_t status = mESProcessor.SetDiscardTrigger(start_trigger);
    EXPECT_EQ(PlayerNoError, status);

    // ES Processor should now be in StartDiscardSet state
    // buffers pushed should be forwarded to the connected input port
    buffer1.mCodedFrameParams.PlaybackTime = 80000;
    buffer1.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &buffer1);
    EXPECT_EQ(RingNoError, status);

    // Set a end trigger on PTS
    stm_se_play_stream_discard_trigger_t end_trigger;
    end_trigger.type          = STM_SE_PLAY_STREAM_PTS_TRIGGER;
    end_trigger.start_not_end = false;
    end_trigger.u.pts_trigger.pts = 160000;
    end_trigger.u.pts_trigger.tolerance = 0;

    status = mESProcessor.SetDiscardTrigger(end_trigger);
    EXPECT_EQ(PlayerNoError, status);

    // ES Processor should now be in StartEndDiscardSet state
    // buffers pushed should be forwarded to the connected input port until
    // marker frame is detected
    buffer2.mCodedFrameParams.PlaybackTime = 100000;
    buffer2.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &buffer2);
    EXPECT_EQ(RingNoError, status);

    // Push a splicing discontinuity marker frame
    markerFrame.mSequenceNumber.MarkerFrame = true;
    markerFrame.mSequenceNumber.MarkerType  = SplicingMarker;
    markerFrame.mSequenceNumber.SplicingMarkerData.PTS_offset = 10000;
    status = mESProcessor.Insert((uintptr_t) &markerFrame);
    EXPECT_EQ(RingNoError, status);

    // Now push a buffer with the right PTS
    // The ES processor should now discard any buffer pushed until
    // end Trigger PTS is detected
    discarded1.mCodedFrameParams.PlaybackTime = 120000;
    discarded1.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &discarded1);
    EXPECT_EQ(RingNoError, status);

    // Reset the pending trigger, EsProcessor should end discarding.
    status = mESProcessor.Reset();
    EXPECT_EQ(PlayerNoError, status);

    buffer3.mCodedFrameParams.PlaybackTime = 140000;
    buffer3.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &buffer3);
    EXPECT_EQ(RingNoError, status);
}

TEST_F(ES_Processor_cTest, CancelEndTriggerInDiskToLiveTransition) {
    MockBuffer_c buffer1(mMockPlayer), buffer2(mMockPlayer), buffer3(mMockPlayer); // buffer that should be forwarded by ES Processor
    MockBuffer_c discarded1(mMockPlayer), discarded2(mMockPlayer), discarded3(mMockPlayer);// buffer that should be discarded by ES Processor
    MockBuffer_c markerFrame(mMockPlayer);

    {
        InSequence s;

        // mMockPort expectations
        EXPECT_CALL(mMockPort, Insert(IsCorrectBuffer(&buffer1, 80000)))
        .WillOnce(Return(RingNoError));
        EXPECT_CALL(mMockPort, Insert(IsCorrectBuffer(&buffer2, 100000)))
        .WillOnce(Return(RingNoError));
        EXPECT_CALL(mMockStream, SignalEvent(IsSplicingMarkerDiscardingEvent(true)))
        .WillOnce(Return(PlayerNoError));
        EXPECT_CALL(mMockStream, SignalEvent(IsPTSDiscardingEvent(false)))
        .WillOnce(Return(PlayerNoError));
        EXPECT_CALL(mMockPort, Insert(IsCorrectBuffer(&buffer3, 180000)))
        .WillOnce(Return(RingNoError));
    }

    // Set start trigger on splicing discontinuity
    stm_se_play_stream_discard_trigger_t start_trigger;
    start_trigger.type          = STM_SE_PLAY_STREAM_SPLICING_MARKER_TRIGGER;
    start_trigger.start_not_end = true;

    PlayerStatus_t status = mESProcessor.SetDiscardTrigger(start_trigger);
    EXPECT_EQ(PlayerNoError, status);

    // ES Processor should now be in StartDiscardSet state
    // buffers pushed should be forwarded to the connected input port
    buffer1.mCodedFrameParams.PlaybackTime = 80000;
    buffer1.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &buffer1);
    EXPECT_EQ(RingNoError, status);

    // Set a end trigger on PTS
    stm_se_play_stream_discard_trigger_t end_trigger;
    end_trigger.type          = STM_SE_PLAY_STREAM_PTS_TRIGGER;
    end_trigger.start_not_end = false;
    end_trigger.u.pts_trigger.pts = 140000;
    end_trigger.u.pts_trigger.tolerance = 0;

    status = mESProcessor.SetDiscardTrigger(end_trigger);
    EXPECT_EQ(PlayerNoError, status);

    // ES Processor should now be in StartEndDiscardSet state
    // buffers pushed should be forwarded to the connected input port until
    // marker frame is detected
    buffer2.mCodedFrameParams.PlaybackTime = 100000;
    buffer2.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &buffer2);
    EXPECT_EQ(RingNoError, status);

    // Cancel End trigger to revert back to StartDiscardSet state
    stm_se_play_stream_discard_trigger_t cancel_trigger;
    cancel_trigger.type          = STM_SE_PLAY_STREAM_CANCEL_TRIGGER;
    cancel_trigger.start_not_end = false;

    status = mESProcessor.SetDiscardTrigger(cancel_trigger);
    EXPECT_EQ(PlayerNoError, status);

    // Set same End trigger as the one previously configured
    end_trigger.type          = STM_SE_PLAY_STREAM_PTS_TRIGGER;
    end_trigger.start_not_end = false;
    end_trigger.u.pts_trigger.pts = 140000;
    end_trigger.u.pts_trigger.tolerance = 0;

    status = mESProcessor.SetDiscardTrigger(end_trigger);
    EXPECT_EQ(PlayerNoError, status);

    // Push a splicing discontinuity marker frame
    markerFrame.mSequenceNumber.MarkerFrame = true;
    markerFrame.mSequenceNumber.MarkerType  = SplicingMarker;
    markerFrame.mSequenceNumber.SplicingMarkerData.PTS_offset = 0;
    status = mESProcessor.Insert((uintptr_t) &markerFrame);
    EXPECT_EQ(RingNoError, status);

    // Now push a buffer with the right PTS
    // The ES processor should now discard any buffer pushed until
    // end Trigger PTS is detected
    discarded1.mCodedFrameParams.PlaybackTime = 120000;
    discarded1.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &discarded1);
    EXPECT_EQ(RingNoError, status);

    // Cancel end trigger, EsProcessor should keep discarding forever
    // until new end trigger is received
    cancel_trigger.type          = STM_SE_PLAY_STREAM_CANCEL_TRIGGER;
    cancel_trigger.start_not_end = false;

    status = mESProcessor.SetDiscardTrigger(cancel_trigger);
    EXPECT_EQ(PlayerNoError, status);

    discarded2.mCodedFrameParams.PlaybackTime = 140000;
    discarded2.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &discarded2);
    EXPECT_EQ(RingNoError, status);

    // Set a new end trigger on PTS
    end_trigger.type          = STM_SE_PLAY_STREAM_PTS_TRIGGER;
    end_trigger.start_not_end = false;
    end_trigger.u.pts_trigger.pts = 180000;
    end_trigger.u.pts_trigger.tolerance = 0;

    status = mESProcessor.SetDiscardTrigger(end_trigger);
    EXPECT_EQ(PlayerNoError, status);

    discarded3.mCodedFrameParams.PlaybackTime = 140000;
    discarded3.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &discarded3);
    EXPECT_EQ(RingNoError, status);

    buffer3.mCodedFrameParams.PlaybackTime = 180000;
    buffer3.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &buffer3);
    EXPECT_EQ(RingNoError, status);
}

TEST_F(ES_Processor_cTest, CancelStartTriggerInDiskToLiveTransition) {
    MockBuffer_c buffer1(mMockPlayer), buffer2(mMockPlayer), buffer3(mMockPlayer); // buffer that should be forwarded by ES Processor
    MockBuffer_c markerFrame(mMockPlayer);

    {
        InSequence s;

        // mMockPort expectations
        EXPECT_CALL(mMockPort, Insert(IsCorrectBuffer(&buffer1, 80000)))
        .WillOnce(Return(RingNoError));
        EXPECT_CALL(mMockPort, Insert(IsCorrectBuffer(&buffer2, 100000)))
        .WillOnce(Return(RingNoError));
        EXPECT_CALL(mMockPort, Insert(IsCorrectBuffer(&buffer3, 120000)))
        .WillOnce(Return(RingNoError));
    }

    // Set start trigger on splicing discontinuity
    stm_se_play_stream_discard_trigger_t start_trigger;
    start_trigger.type          = STM_SE_PLAY_STREAM_SPLICING_MARKER_TRIGGER;
    start_trigger.start_not_end = true;

    PlayerStatus_t status = mESProcessor.SetDiscardTrigger(start_trigger);
    EXPECT_EQ(PlayerNoError, status);

    // ES Processor should now be in StartDiscardSet state
    // buffers pushed should be forwarded to the connected input port
    buffer1.mCodedFrameParams.PlaybackTime = 80000;
    buffer1.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &buffer1);
    EXPECT_EQ(RingNoError, status);

    // Set a end trigger on PTS
    stm_se_play_stream_discard_trigger_t end_trigger;
    end_trigger.type          = STM_SE_PLAY_STREAM_PTS_TRIGGER;
    end_trigger.start_not_end = false;
    end_trigger.u.pts_trigger.pts = 140000;
    end_trigger.u.pts_trigger.tolerance = 0;

    status = mESProcessor.SetDiscardTrigger(end_trigger);
    EXPECT_EQ(PlayerNoError, status);

    // ES Processor should now be in StartEndDiscardSet state
    // buffers pushed should be forwarded to the connected input port until
    // marker frame is detected
    buffer2.mCodedFrameParams.PlaybackTime = 100000;
    buffer2.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &buffer2);
    EXPECT_EQ(RingNoError, status);

    // Cancel start trigger, EsProcessor should go in default state
    // both triggers are canceled in this case as we do not have
    // Processing state with only End trigger configured
    stm_se_play_stream_discard_trigger_t cancel_trigger;
    cancel_trigger.type          = STM_SE_PLAY_STREAM_CANCEL_TRIGGER;
    cancel_trigger.start_not_end = true;

    status = mESProcessor.SetDiscardTrigger(cancel_trigger);
    EXPECT_EQ(PlayerNoError, status);

    buffer3.mCodedFrameParams.PlaybackTime = 120000;
    buffer3.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &buffer3);
    EXPECT_EQ(RingNoError, status);
}

TEST_F(ES_Processor_cTest, CancelStartTriggerInLiveToDiskTransition) {
    MockBuffer_c buffer1(mMockPlayer), buffer2(mMockPlayer), buffer3(mMockPlayer); // buffer that should be forwarded by ES Processor
    MockBuffer_c markerFrame(mMockPlayer);

    {
        InSequence s;

        // mMockPort expectations
        EXPECT_CALL(mMockPort, Insert(IsCorrectBuffer(&buffer1, 80000)))
        .WillOnce(Return(RingNoError));
        EXPECT_CALL(mMockPort, Insert(IsCorrectBuffer(&buffer2, 100000)))
        .WillOnce(Return(RingNoError));
        EXPECT_CALL(mMockPort, Insert(IsCorrectBuffer(&buffer3, 120000)))
        .WillOnce(Return(RingNoError));
    }

    // Set a start trigger on PTS
    stm_se_play_stream_discard_trigger_t start_trigger;
    start_trigger.type          = STM_SE_PLAY_STREAM_PTS_TRIGGER;
    start_trigger.start_not_end = true;
    start_trigger.u.pts_trigger.pts = 120000;
    start_trigger.u.pts_trigger.tolerance = 0;

    PlayerStatus_t status = mESProcessor.SetDiscardTrigger(start_trigger);
    EXPECT_EQ(PlayerNoError, status);

    // ES Processor should now be in StartDiscardSet state
    // buffers pushed should be forwarded to the connected input port
    buffer1.mCodedFrameParams.PlaybackTime = 80000;
    buffer1.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &buffer1);
    EXPECT_EQ(RingNoError, status);

    buffer2.mCodedFrameParams.PlaybackTime = 100000;
    buffer2.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &buffer2);
    EXPECT_EQ(RingNoError, status);

    // Cancel start trigger, EsProcessor should go in default state
    stm_se_play_stream_discard_trigger_t cancel_trigger;
    cancel_trigger.type          = STM_SE_PLAY_STREAM_CANCEL_TRIGGER;
    cancel_trigger.start_not_end = true;

    status = mESProcessor.SetDiscardTrigger(cancel_trigger);
    EXPECT_EQ(PlayerNoError, status);

    // Discarding is over and buffer should flow
    buffer3.mCodedFrameParams.PlaybackTime = 120000;
    buffer3.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &buffer3);
    EXPECT_EQ(RingNoError, status);
}

// Tests with on purpose erroneous sequences
TEST_F(ES_Processor_cTest, ErroneousApiCallsInLiveToDiskToLiveTransitions) {
    MockBuffer_c buffer1(mMockPlayer), buffer2(mMockPlayer), buffer3(mMockPlayer), buffer4(mMockPlayer), buffer5(mMockPlayer); // buffer that should be forwarded by ES Processor
    MockBuffer_c discarded1(mMockPlayer), discarded2(mMockPlayer), discarded3(mMockPlayer), discarded4(mMockPlayer); // buffer that should be discarded by ES Processor
    MockBuffer_c markerFrame(mMockPlayer);

    {
        InSequence s;

        // mMockPort expectations
        EXPECT_CALL(mMockPort, Insert(IsCorrectBuffer(&buffer1, 80000)))
        .WillOnce(Return(RingNoError));
        EXPECT_CALL(mMockPort, Insert(IsCorrectBuffer(&buffer2, 100000)))
        .WillOnce(Return(RingNoError));
        EXPECT_CALL(mMockStream, SignalEvent(IsPTSDiscardingEvent(true)))
        .WillOnce(Return(PlayerNoError));
        EXPECT_CALL(mMockStream, SignalEvent(IsSplicingMarkerDiscardingEvent(false)))
        .WillOnce(Return(PlayerNoError));
        EXPECT_CALL(mMockPort, Insert(IsCorrectBuffer(&buffer3, 160000)))
        .WillOnce(Return(RingNoError));
        EXPECT_CALL(mMockStream, SignalEvent(IsSplicingMarkerDiscardingEvent(true)))
        .WillOnce(Return(PlayerNoError));
        EXPECT_CALL(mMockStream, SignalEvent(IsPTSDiscardingEvent(false)))
        .WillOnce(Return(PlayerNoError));
        EXPECT_CALL(mMockPort, Insert(IsCorrectBuffer(&buffer4, 225000)))
        .WillOnce(Return(RingNoError));
    }

    // Erroneous: Cancel Start & end trigger without being previously set
    stm_se_play_stream_discard_trigger_t end_trigger;
    end_trigger.type          = STM_SE_PLAY_STREAM_CANCEL_TRIGGER;
    end_trigger.start_not_end = false;

    PlayerStatus_t status = mESProcessor.SetDiscardTrigger(end_trigger);
    EXPECT_EQ(PlayerError, status);

    stm_se_play_stream_discard_trigger_t start_trigger;
    start_trigger.type          = STM_SE_PLAY_STREAM_CANCEL_TRIGGER;
    start_trigger.start_not_end = true;

    status = mESProcessor.SetDiscardTrigger(start_trigger);
    EXPECT_EQ(PlayerError, status);

    // Erroneous: Set end trigger on splicing discontinuity
    // Start trigger should be set first
    end_trigger.type          = STM_SE_PLAY_STREAM_SPLICING_MARKER_TRIGGER;
    end_trigger.start_not_end = false;

    status = mESProcessor.SetDiscardTrigger(end_trigger);
    EXPECT_EQ(PlayerError, status);

    // Set a start trigger on PTS
    start_trigger.type          = STM_SE_PLAY_STREAM_PTS_TRIGGER;
    start_trigger.start_not_end = true;
    start_trigger.u.pts_trigger.pts = 120000;
    start_trigger.u.pts_trigger.tolerance = 0;

    status = mESProcessor.SetDiscardTrigger(start_trigger);
    EXPECT_EQ(PlayerNoError, status);

    // ES Processor should now be in StartDiscardSet state
    // buffers pushed should be forwarded to the connected input port

    // Erroneous: Set another start trigger on PTS in this state.
    start_trigger.type          = STM_SE_PLAY_STREAM_PTS_TRIGGER;
    start_trigger.start_not_end = true;

    status = mESProcessor.SetDiscardTrigger(start_trigger);
    EXPECT_EQ(PlayerError, status);

    buffer1.mCodedFrameParams.PlaybackTime = 80000;
    buffer1.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &buffer1);
    EXPECT_EQ(RingNoError, status);

    // Erroneous: Cancel end trigger without being previously set
    end_trigger.type          = STM_SE_PLAY_STREAM_CANCEL_TRIGGER;
    end_trigger.start_not_end = false;

    status = mESProcessor.SetDiscardTrigger(end_trigger);
    EXPECT_EQ(PlayerError, status);

    // Set end trigger on splicing discontinuity
    end_trigger.type          = STM_SE_PLAY_STREAM_SPLICING_MARKER_TRIGGER;
    end_trigger.start_not_end = false;

    status = mESProcessor.SetDiscardTrigger(end_trigger);
    EXPECT_EQ(PlayerNoError, status);

    // Erroneous: Set end trigger on splicing discontinuity
    end_trigger.type          = STM_SE_PLAY_STREAM_SPLICING_MARKER_TRIGGER;
    end_trigger.start_not_end = false;

    status = mESProcessor.SetDiscardTrigger(end_trigger);
    EXPECT_EQ(PlayerError, status);

    // Erroneous: Set another start trigger on PTS in this state.
    start_trigger.type          = STM_SE_PLAY_STREAM_PTS_TRIGGER;
    start_trigger.start_not_end = true;

    status = mESProcessor.SetDiscardTrigger(start_trigger);
    EXPECT_EQ(PlayerError, status);

    // ES Processor should now be in StartEndDiscardSet state
    // buffers pushed should be forwarded to the connected input port until
    // discard PTS is received
    buffer2.mCodedFrameParams.PlaybackTime = 100000;
    buffer2.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &buffer2);
    EXPECT_EQ(RingNoError, status);

    // Now push a buffer with the right PTS
    // The ES processor should now discard any buffer pushed until
    // marker frame is received
    discarded1.mCodedFrameParams.PlaybackTime = start_trigger.u.pts_trigger.pts;
    discarded1.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &discarded1);
    EXPECT_EQ(RingNoError, status);

    // Erroneous: Set another start trigger on PTS in this state.
    start_trigger.type          = STM_SE_PLAY_STREAM_PTS_TRIGGER;
    start_trigger.start_not_end = true;

    status = mESProcessor.SetDiscardTrigger(start_trigger);
    EXPECT_EQ(PlayerError, status);

    // Erroneous: Cancel Start trigger in EndDiscardSet state
    start_trigger.type          = STM_SE_PLAY_STREAM_CANCEL_TRIGGER;
    start_trigger.start_not_end = true;

    status = mESProcessor.SetDiscardTrigger(start_trigger);
    EXPECT_EQ(PlayerError, status);

    discarded2.mCodedFrameParams.PlaybackTime = 140000;
    discarded2.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &discarded2);
    EXPECT_EQ(RingNoError, status);

    // Push a splicing discontinuity marker frame
    markerFrame.mSequenceNumber.MarkerFrame = true;
    markerFrame.mSequenceNumber.MarkerType  = SplicingMarker;
    markerFrame.mSequenceNumber.SplicingMarkerData.PTS_offset = 40000;
    status = mESProcessor.Insert((uintptr_t) &markerFrame);
    EXPECT_EQ(RingNoError, status);

    // Discarding is over and buffer should flow
    buffer3.mCodedFrameParams.PlaybackTime = 120000;
    buffer3.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &buffer3);
    EXPECT_EQ(RingNoError, status);

    // Set a start trigger Splicing.
    start_trigger.type          = STM_SE_PLAY_STREAM_SPLICING_MARKER_TRIGGER;
    start_trigger.start_not_end = true;

    status = mESProcessor.SetDiscardTrigger(start_trigger);
    EXPECT_EQ(PlayerNoError, status);

    // Push a splicing discontinuity marker frame
    markerFrame.mSequenceNumber.MarkerFrame = true;
    markerFrame.mSequenceNumber.MarkerType  = SplicingMarker;
    markerFrame.mSequenceNumber.SplicingMarkerData.PTS_offset = -40000;
    status = mESProcessor.Insert((uintptr_t) &markerFrame);
    EXPECT_EQ(RingNoError, status);

    discarded3.mCodedFrameParams.PlaybackTime = 180000;
    discarded3.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &discarded3);
    EXPECT_EQ(RingNoError, status);

    // Erroneous:  Set a start trigger in DiscardingState_NoEndDiscardSet.
    start_trigger.type          = STM_SE_PLAY_STREAM_SPLICING_MARKER_TRIGGER;
    start_trigger.start_not_end = true;

    status = mESProcessor.SetDiscardTrigger(start_trigger);
    EXPECT_EQ(PlayerError, status);

    // Erroneous: Cancel end trigger without being previously set
    end_trigger.type          = STM_SE_PLAY_STREAM_CANCEL_TRIGGER;
    end_trigger.start_not_end = false;

    status = mESProcessor.SetDiscardTrigger(end_trigger);
    EXPECT_EQ(PlayerError, status);

    // Set a end trigger on PTS
    end_trigger.type          = STM_SE_PLAY_STREAM_PTS_TRIGGER;
    end_trigger.start_not_end = false;
    end_trigger.u.pts_trigger.pts = 220000;
    end_trigger.u.pts_trigger.tolerance = 10000;

    status = mESProcessor.SetDiscardTrigger(end_trigger);
    EXPECT_EQ(PlayerNoError, status);

    discarded4.mCodedFrameParams.PlaybackTime = 219999;
    discarded4.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &discarded4);
    EXPECT_EQ(RingNoError, status);

    // Discarding is over and buffer should flow
    buffer4.mCodedFrameParams.PlaybackTime = 225000;
    buffer4.mCodedFrameParams.PlaybackTimeValid = true;
    status = mESProcessor.Insert((uintptr_t) &buffer4);
    EXPECT_EQ(RingNoError, status);
}

