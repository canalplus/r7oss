#ifndef _DECODE_BUFFER_MANAGER_H
#define _DECODE_BUFFER_MANAGER_H

#include "gmock/gmock.h"
#include "decode_buffer_manager.h"


class MockDecodeBufferManager_c : public DecodeBufferManager_c {
  public:
    MockDecodeBufferManager_c();
    virtual ~MockDecodeBufferManager_c();

    MOCK_METHOD0(ResetDecoderMap,
                 void());

    MOCK_METHOD0(CreateDecoderMap,
                 DecodeBufferManagerStatus_t(void));

    MOCK_METHOD1(GetDecodeBufferPool,
                 DecodeBufferManagerStatus_t(BufferPool_t *Pool));

    MOCK_METHOD0(EnterStreamSwitch,
                 DecodeBufferManagerStatus_t(void));

    MOCK_METHOD3(FillOutDefaultList,
                 DecodeBufferManagerStatus_t(BufferFormat_t PrimaryFormat, DecodeBufferComponentElementMask_t Elements, DecodeBufferComponentDescriptor_t *List));

    MOCK_METHOD1(InitializeComponentList,
                 DecodeBufferManagerStatus_t(DecodeBufferComponentDescriptor_t *List));

    MOCK_METHOD1(InitializeSimpleComponentList,
                 DecodeBufferManagerStatus_t(BufferFormat_t Type));

    MOCK_METHOD2(ModifyComponentFormat,
                 DecodeBufferManagerStatus_t(DecodeBufferComponentType_t Component, BufferFormat_t NewFormat));

    MOCK_METHOD2(ComponentEnable,
                 DecodeBufferManagerStatus_t(DecodeBufferComponentType_t Component, bool Enabled));
    MOCK_METHOD1(ArchiveReservedMemory,
                 DecodeBufferManagerStatus_t(DecodeBufferReservedBlock_t *Blocks));
    MOCK_METHOD1(InitializeReservedMemory,
                 DecodeBufferManagerStatus_t(DecodeBufferReservedBlock_t *Blocks));
    MOCK_METHOD2(ReserveMemory,
                 DecodeBufferManagerStatus_t(void *Base, unsigned int Size));
    MOCK_METHOD0(AbortGetBuffer,
                 DecodeBufferManagerStatus_t(void));
    MOCK_METHOD0(DisplayComponents,
                 void());
    MOCK_METHOD2(GetDecodeBuffer,
                 DecodeBufferManagerStatus_t(DecodeBufferRequest_t *Request, Buffer_t *Buffer));
    MOCK_METHOD1(EnsureReferenceComponentsPresent,
                 DecodeBufferManagerStatus_t(Buffer_t Buffer));
    MOCK_METHOD2(GetEstimatedBufferCount,
                 DecodeBufferManagerStatus_t(unsigned int *Count, DecodeBufferUsage_t For));
    MOCK_METHOD2(ReleaseBuffer,
                 DecodeBufferManagerStatus_t(Buffer_t Buffer, bool ForReference));
    MOCK_METHOD2(ReleaseBufferComponents,
                 DecodeBufferManagerStatus_t(Buffer_t Buffer, DecodeBufferUsage_t ForUse));
    MOCK_METHOD1(IncrementReferenceUseCount,
                 DecodeBufferManagerStatus_t(Buffer_t Buffer));
    MOCK_METHOD1(DecrementReferenceUseCount,
                 DecodeBufferManagerStatus_t(Buffer_t Buffer));
    MOCK_METHOD1(ComponentDataType,
                 BufferFormat_t(DecodeBufferComponentType_t Component));
    MOCK_METHOD2(ComponentBorder,
                 unsigned int(DecodeBufferComponentType_t Component, unsigned int Index));
    MOCK_METHOD2(ComponentAddress,
                 void *(DecodeBufferComponentType_t Component, AddressType_t AlternateAddressType));
    MOCK_METHOD2(ComponentPresent,
                 bool(Buffer_t Buffer, DecodeBufferComponentType_t Component));
    MOCK_METHOD2(ComponentSize,
                 unsigned int(Buffer_t Buffer, DecodeBufferComponentType_t Component));
    MOCK_METHOD2(ComponentBaseAddress,
                 void *(Buffer_t Buffer, DecodeBufferComponentType_t Component));
    MOCK_METHOD3(ComponentBaseAddress,
                 void *(Buffer_t Buffer, DecodeBufferComponentType_t Component, AddressType_t AlternateAddressType));
    MOCK_METHOD3(ComponentDimension,
                 unsigned int(Buffer_t Buffer, DecodeBufferComponentType_t Component, unsigned int DimensionIndex));
    MOCK_METHOD4(ComponentStride,
                 unsigned int(Buffer_t Buffer, DecodeBufferComponentType_t Component, unsigned int DimensionIndex, unsigned int ComponentIndex));
    MOCK_METHOD2(DecimationFactor,
                 unsigned int(Buffer_t Buffer, unsigned int DecimationIndex));
    MOCK_METHOD2(LumaSize,
                 unsigned int(Buffer_t Buffer, DecodeBufferComponentType_t Component));
    MOCK_METHOD2(Luma,
                 unsigned char *(Buffer_t Buffer, DecodeBufferComponentType_t Component));
    MOCK_METHOD3(Luma,
                 unsigned char *(Buffer_t Buffer, DecodeBufferComponentType_t Component, AddressType_t AlternateAddressType));
    MOCK_METHOD2(LumaInsideBorder,
                 unsigned char *(Buffer_t Buffer, DecodeBufferComponentType_t Component));
    MOCK_METHOD3(LumaInsideBorder,
                 unsigned char *(Buffer_t Buffer, DecodeBufferComponentType_t Component, AddressType_t AlternateAddressType));
    MOCK_METHOD2(ChromaSize,
                 unsigned int(Buffer_t Buffer, DecodeBufferComponentType_t Component));
    MOCK_METHOD2(Chroma,
                 unsigned char *(Buffer_t Buffer, DecodeBufferComponentType_t Component));
    MOCK_METHOD3(Chroma,
                 unsigned char *(Buffer_t Buffer, DecodeBufferComponentType_t Component, AddressType_t AlternateAddressType));
    MOCK_METHOD2(ChromaInsideBorder,
                 unsigned char *(Buffer_t Buffer, DecodeBufferComponentType_t Component));
    MOCK_METHOD3(ChromaInsideBorder,
                 unsigned char *(Buffer_t Buffer, DecodeBufferComponentType_t Component, AddressType_t AlternateAddressType));
    MOCK_METHOD2(RasterSize,
                 unsigned int(Buffer_t Buffer, DecodeBufferComponentType_t Component));
    MOCK_METHOD2(Raster,
                 unsigned char *(Buffer_t Buffer, DecodeBufferComponentType_t Component));
    MOCK_METHOD3(Raster,
                 unsigned char *(Buffer_t Buffer, DecodeBufferComponentType_t Component, AddressType_t AlternateAddressType));
};


#endif
