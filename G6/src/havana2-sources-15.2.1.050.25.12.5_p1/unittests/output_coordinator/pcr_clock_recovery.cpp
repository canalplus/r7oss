#include "output_coordinator_ctest.h"

class OutputCoordinator_ClockRecovery_cTest: public OutputCoordinator_cTest {
  protected:

    OutputCoordinator_ClockRecovery_cTest() : mPCRDataFile(0) {}

    virtual void TearDown() {
        if (mPCRDataFile) {
            fclose(mPCRDataFile);
            mPCRDataFile = NULL;
        }
    }

    void setPCRDataFile(const char *filename);
    void replayOneClockRecoveryIntegration();

    FILE                    *mPCRDataFile;
};

void OutputCoordinator_ClockRecovery_cTest::setPCRDataFile(const char *filename) {
    if (mPCRDataFile) {
        fclose(mPCRDataFile);
    }
    mPCRDataFile = fopen(filename, "rb");
    ASSERT_TRUE(mPCRDataFile != NULL);
}

void OutputCoordinator_ClockRecovery_cTest::replayOneClockRecoveryIntegration() {
    OutputCoordinatorStatus_t error;
    unsigned int accumulatedPoints = 0;

    while (!feof(mPCRDataFile)) {
        struct data_point_s {
            uint32_t Playback;
            uint32_t padding;
            uint64_t          SourceTime;
            uint64_t          SystemTime;
            uint8_t           NewSequence;
            uint32_t padding2;
        } DataPoint;

        size_t n = fread(&DataPoint, 1, sizeof(DataPoint) , mPCRDataFile);
        ASSERT_EQ(sizeof(DataPoint), n);

        if (DataPoint.NewSequence) {
            error = mOutputCoordinator.ClockRecoveryInitialize(TimeFormatPts);
            EXPECT_EQ(OutputCoordinatorNoError, error);
        }

        error = mOutputCoordinator.ClockRecoveryDataPoint(DataPoint.SourceTime, DataPoint.SystemTime);
        EXPECT_EQ(OutputCoordinatorNoError, error);

        if (mMockPlayback.mStatistics.ClockRecoveryAccumulatedPoints < accumulatedPoints) {
            return;
        }
        accumulatedPoints = mMockPlayback.mStatistics.ClockRecoveryAccumulatedPoints;
    }
}

TEST_F(OutputCoordinator_ClockRecovery_cTest, ClockRecoveryInitialization) {

    OutputCoordinatorStatus_t error;

    // injecting a ClockDataPoint before initialization should return an error
    error = mOutputCoordinator.ClockRecoveryDataPoint(50000000ull, 8000000ull);
    EXPECT_EQ(OutputCoordinatorError, error);

    bool initialized;
    error = mOutputCoordinator.ClockRecoveryIsInitialized(&initialized);
    EXPECT_EQ(OutputCoordinatorNoError, error);
    EXPECT_EQ(false, initialized);

    error = mOutputCoordinator.ClockRecoveryInitialize(TimeFormatPts);
    EXPECT_EQ(OutputCoordinatorNoError, error);

    error = mOutputCoordinator.ClockRecoveryIsInitialized(&initialized);
    EXPECT_EQ(OutputCoordinatorNoError, error);
    EXPECT_EQ(true, initialized);
}

class ClockRecoveryNominalTest: public OutputCoordinator_ClockRecovery_cTest,
    public ::testing::WithParamInterface<const char *> {

};



TEST_P(ClockRecoveryNominalTest, ClockRecoveryNominal) {

    //-----------------------------------------------------------------
    // Some tests using a file with stable gradient and within spec
    // and no jitter on the data points
    //-----------------------------------------------------------------

    //setPCRDataFile("output_coordinator/pcr_data/Cornell_PCR.raw");
    setPCRDataFile(GetParam());

    replayOneClockRecoveryIntegration();

    EXPECT_EQ(0u, mMockPlayback.mStatistics.ClockRecoveryCummulativeDiscardedPoints);
    EXPECT_EQ(0u, mMockPlayback.mStatistics.ClockRecoveryCummulativeDiscardResets);
    EXPECT_EQ(-25, mMockPlayback.mStatistics.ClockRecoveryActualGradient);

    unsigned int integrationTimeWindow = mMockPlayback.mStatistics.ClockRecoveryIntegrationTimeWindow;

    replayOneClockRecoveryIntegration();

    EXPECT_EQ(0u, mMockPlayback.mStatistics.ClockRecoveryCummulativeDiscardedPoints);
    EXPECT_EQ(0u, mMockPlayback.mStatistics.ClockRecoveryCummulativeDiscardResets);
    EXPECT_EQ(-25, mMockPlayback.mStatistics.ClockRecoveryActualGradient);
    EXPECT_EQ(integrationTimeWindow * 2, mMockPlayback.mStatistics.ClockRecoveryIntegrationTimeWindow);

    integrationTimeWindow = mMockPlayback.mStatistics.ClockRecoveryIntegrationTimeWindow;

    replayOneClockRecoveryIntegration();

    EXPECT_EQ(0u, mMockPlayback.mStatistics.ClockRecoveryCummulativeDiscardedPoints);
    EXPECT_EQ(0u, mMockPlayback.mStatistics.ClockRecoveryCummulativeDiscardResets);
    EXPECT_EQ(-26, mMockPlayback.mStatistics.ClockRecoveryActualGradient);
    EXPECT_EQ(integrationTimeWindow * 2, mMockPlayback.mStatistics.ClockRecoveryIntegrationTimeWindow);
}

INSTANTIATE_TEST_CASE_P(
    ClockRecoveryNominalTests,
    ClockRecoveryNominalTest,
    ::testing::Values("output_coordinator/pcr_data/Cornell_PCR.raw"));

TEST_F(OutputCoordinator_ClockRecovery_cTest, ClockRecoveryGradientOutOfSpec) {

    //-----------------------------------------------------------------
    // Some tests using a file with gradient out of spec
    //-----------------------------------------------------------------
    setPCRDataFile("output_coordinator/pcr_data/bug29773.raw");

    replayOneClockRecoveryIntegration();

    int establishedGradient = mMockPlayback.mStatistics.ClockRecoveryEstablishedGradient;
    int clockAdjustment = mMockPlayback.mStatistics.ClockRecoveryClockAdjustment;

    replayOneClockRecoveryIntegration();

    EXPECT_EQ(0u, mMockPlayback.mStatistics.ClockRecoveryCummulativeDiscardedPoints);
    EXPECT_EQ(0u, mMockPlayback.mStatistics.ClockRecoveryCummulativeDiscardResets);
    // since the gradient is out of spec, values should not be updated
    EXPECT_EQ(establishedGradient, mMockPlayback.mStatistics.ClockRecoveryEstablishedGradient);
    EXPECT_EQ(clockAdjustment, mMockPlayback.mStatistics.ClockRecoveryClockAdjustment);
}

