#ifndef _MOCK_BUFFER_POOL_H_
#define _MOCK_BUFFER_POOL_H_

#include "gmock/gmock.h"
#include "buffer_pool.h"

class MockBufferPool_c : public BufferPool_c {
  public:
    MockBufferPool_c();
    virtual ~MockBufferPool_c();

    MOCK_METHOD5(AttachMetaData,
                 BufferStatus_t(MetaDataType_t Type, unsigned int Size, void *MemoryPool,
                                void **ArrayOfMemoryBlocks, char *DeviceMemoryPartitionName));

    MOCK_METHOD1(DetachMetaData,
                 void(MetaDataType_t Type));

    MOCK_METHOD6(GetBuffer,
                 BufferStatus_t(Buffer_t *Buffer, unsigned int OwnerIdentifier,
                                unsigned int RequiredSize, bool NonBlocking, bool RequiredSizeIsLowerBound, uint32_t TimeOut));

    MOCK_METHOD0(AbortBlockingGetBuffer,
                 void(void));

    MOCK_METHOD1(SetMemoryAccessType,
                 void(unsigned int));

    MOCK_METHOD1(GetType,
                 void(BufferType_t *Type));

    MOCK_METHOD6(GetPoolUsage,
                 void(unsigned int *BuffersInPool, unsigned int *BuffersWithNonZeroReferenceCount,
                      unsigned int *MemoryInPool, unsigned int *MemoryAllocated, unsigned int *MemoryInUse, unsigned int *LargestFreeMemoryBlock));

    MOCK_METHOD3(GetAllUsedBuffers,
                 BufferStatus_t(unsigned int ArraySize, Buffer_t *ArrayOfBuffers, unsigned int OwnerIdentifier));

    MOCK_METHOD1(Dump,
                 void(unsigned int Flags));
};

#endif
