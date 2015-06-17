#ifndef _MOCK_MANIFESTATTION_COORDINATOR_H
#define _MOCK_MANIFESTATTION_COORDINATOR_H

#include "gmock/gmock.h"
#include "manifestation_coordinator.h"

class MockManifestationCoordinator_c: public ManifestationCoordinator_c {
  public:
    MockManifestationCoordinator_c();
    virtual ~MockManifestationCoordinator_c();

    MOCK_METHOD2(AddManifestation,
                 ManifestationCoordinatorStatus_t (Manifestor_t Manifestor, void *Identifier));
    MOCK_METHOD2(RemoveManifestation,
                 ManifestationCoordinatorStatus_t (Manifestor_t Manifestor, void *Identifier));
    MOCK_METHOD2(GetSurfaceParameters,
                 ManifestationCoordinatorStatus_t (OutputSurfaceDescriptor_t *** SurfaceParametersArray, unsigned int *HighestIndex));
    MOCK_METHOD1(GetLastQueuedManifestationTime,
                 ManifestationCoordinatorStatus_t (unsigned long long *Time));
    MOCK_METHOD2(GetNextQueuedManifestationTimes,
                 ManifestationCoordinatorStatus_t (unsigned long long **Times, unsigned long long **Granularities));
    MOCK_METHOD1(Connect,
                 ManifestationCoordinatorStatus_t (Port_c *Port));
    MOCK_METHOD0(ReleaseQueuedDecodeBuffers,
                 ManifestationCoordinatorStatus_t (void));
    MOCK_METHOD1(InitialFrame,
                 ManifestationCoordinatorStatus_t (Buffer_t Buffer));
    MOCK_METHOD1(QueueDecodeBuffer,
                 ManifestationCoordinatorStatus_t (Buffer_t Buffer));
    MOCK_METHOD0(QueueNullManifestation,
                 ManifestationCoordinatorStatus_t (void));
    MOCK_METHOD1(QueueEventSignal,
                 ManifestationCoordinatorStatus_t (PlayerEventRecord_t *Event));
    MOCK_METHOD0(SynchronizeOutput, ManifestationCoordinatorStatus_t (void));
    MOCK_METHOD0(ResetOnStreamSwitch,
                 void (void));
};

#endif
