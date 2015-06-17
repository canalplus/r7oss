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

#ifndef H_DECODE_BUFFER_MANAGER_BASE
#define H_DECODE_BUFFER_MANAGER_BASE

#include "player.h"
#include "allocinline.h"

// The two following partition definitions concern the use case where the decode buffers
// are allocated by the user via STM_SE_CTRL_SHARE_VIDEO_DECODE_BUFFERS compound control.
// AllocatedLaterPartition is used to inform the decode buffer manager that
// the DecodeBufferComponentElement will not have memory partition when initialising play_stream
// The effective partition (IndividualBuffersPartition) will be defined via the compound control.
// to provide the list of individual decode buffers
//
#define  AllocatedLaterPartition    "vid-Allocate-Later"
#define  IndividualBuffersPartition "vid-Allocate-Individual-buffer"
#define  DECODE_BUFFER_HD_SIZE      0x2FD000   // 1920 x 1088 x 1.5

//
// Define the maxinum number of address dedicated to a buffer
// Should be 'Physical', 'cached', 'uncached'
//
#define MAX_BUFFER_ADDRESSING_MODE   3

// ---------------------------------------------------------------------
// Structure to hold the sizes and partition names for components
//

typedef enum
{
    DecodeBufferManagerPartitionData    = BASE_DECODE_BUFFER_MANAGER,
    DecodeBufferManagerPrimaryIndividualBufferList
} DecodeBufferManagerParameterBlockType_t;

//

typedef struct DecodeBufferPartitionTable_s
{
    unsigned int    NumberOfDecodeBuffers;

    unsigned int    PrimaryManifestationPartitionSize;
    char            PrimaryManifestationPartitionName[ALLOCATOR_MAX_PARTITION_NAME_SIZE];

    unsigned int    DecimatedManifestationPartitionSize;
    char            DecimatedManifestationPartitionName[ALLOCATOR_MAX_PARTITION_NAME_SIZE];

    unsigned int    VideoDecodeCopyPartitionSize;
    char            VideoDecodeCopyPartitionName[ALLOCATOR_MAX_PARTITION_NAME_SIZE];

    unsigned int    VideoMacroblockStructurePartitionSize;
    char            VideoMacroblockStructurePartitionName[ALLOCATOR_MAX_PARTITION_NAME_SIZE];

    unsigned int    VideoPostProcessControlPartitionSize;
    char            VideoPostProcessControlPartitionName[ALLOCATOR_MAX_PARTITION_NAME_SIZE];

    unsigned int    AdditionalMemoryBlockPartitionSize;
    char            AdditionalMemoryBlockPartitionName[ALLOCATOR_MAX_PARTITION_NAME_SIZE];
} DecodeBufferPartitionTable_t;

//
typedef struct DecodeBufferPrimaryIndividualTable_s
{
    uint32_t          NumberOfBufs;
    uint32_t          BufSize;
    uint32_t          BufAlignement;
    uint32_t          MemAttribute;   /* bit mask: */
    void             *AllocatorMemory[MAX_DECODE_BUFFERS][MAX_BUFFER_ADDRESSING_MODE];
} DecodeBufferPrimaryIndividualTable_t;

//

typedef struct DecodeBufferManagerParameterBlock_s
{
    DecodeBufferManagerParameterBlockType_t ParameterType;

    union
    {
        DecodeBufferPartitionTable_t         PartitionData;
        DecodeBufferPrimaryIndividualTable_t PrimaryIndividualData;
    };
} DecodeBufferManagerParameterBlock_t;

// ---------------------------------------------------------------------
//
// Structure to the gathered data for each component
//

typedef struct DecodeBufferComponentData_s
{
    unsigned int             AllocationPartitionSize;
    char                     AllocationPartitionName[ALLOCATOR_MAX_PARTITION_NAME_SIZE];

    DecodeBufferComponentDescriptor_t    ComponentDescriptor;

    bool                     Enabled;
    allocator_device_t       AllocatorMemoryDevice;
    union
    {
        void                *AllocatorMemory[MAX_BUFFER_ADDRESSING_MODE];
        struct
        {
            void            *ArrayOfMemoryBlocks[MAX_DECODE_BUFFERS][MAX_BUFFER_ADDRESSING_MODE];
            unsigned int     IndividualBufferSize;
        };
    };
    BufferPool_t             BufferPool;

    unsigned int             LastAllocationSize;

    bool PostponeReset;

} DecodeBufferComponentData_t;


// ---------------------------------------------------------------------
//
// Structure record for each component in an allocated buffer
//

typedef struct DecodeBufferRequestComponentStructure_s
{
    unsigned int             DimensionCount;            // These may be modified by the individual component
    unsigned int             Dimension[MAX_BUFFER_DIMENSIONS];

    unsigned int             ComponentCount;
    unsigned int             ComponentOffset[MAX_BUFFER_COMPONENTS];
    unsigned int             Strides[MAX_BUFFER_DIMENSIONS - 1][MAX_BUFFER_COMPONENTS];
    unsigned int             Size;

    Buffer_t                 Buffer;
    unsigned char           *BufferBase;

    DecodeBufferUsage_t      Usage;

} DecodeBufferRequestComponentStructure_t;


// ---------------------------------------------------------------------
//
// Structure that travels with each and every decode buffer
//

typedef struct DecodeBufferContextRecord_s
{
    Buffer_t                                DecodeBuffer;
    unsigned int                            ReferenceUseCount;
    DecodeBufferRequest_t                   Request;
    DecodeBufferRequestComponentStructure_t StructureComponent[NUMBER_OF_COMPONENTS];
} DecodeBufferContextRecord_t;

class DecodeBufferManager_Base_c : public DecodeBufferManager_c
{
public:
    DecodeBufferManager_Base_c(void);
    ~DecodeBufferManager_Base_c(void);

    //
    // Override for component base class halt/reset functions
    //

    DecodeBufferManagerStatus_t   Halt(void);
    DecodeBufferManagerStatus_t   Reset(void);

    void ResetDecoderMap(void);

    DecodeBufferManagerStatus_t   CreateDecoderMap(void);

    DecodeBufferManagerStatus_t   SetModuleParameters(unsigned int  ParameterBlockSize,
                                                      void         *ParameterBlock);

    //
    // Function to obtain the buffer pool, in order to attach meta data to the whole pool
    //

    DecodeBufferManagerStatus_t   GetDecodeBufferPool(BufferPool_t *Pool);

    //
    // Function to support entry into stream switch mode
    //

    DecodeBufferManagerStatus_t   EnterStreamSwitch(void);

    //
    // Functions to specify the component list, and to enable or
    // disable the inclusion of specific components in a decode buffer.
    //

    DecodeBufferManagerStatus_t   FillOutDefaultList(BufferFormat_t                      PrimaryFormat,
                                                     DecodeBufferComponentElementMask_t  Elements,
                                                     DecodeBufferComponentDescriptor_t   List[]);

    DecodeBufferManagerStatus_t   InitializeComponentList(DecodeBufferComponentDescriptor_t *List);
    DecodeBufferManagerStatus_t   InitializeSimpleComponentList(BufferFormat_t Type);

    DecodeBufferManagerStatus_t   ModifyComponentFormat(DecodeBufferComponentType_t  Component,
                                                        BufferFormat_t               NewFormat);

    DecodeBufferManagerStatus_t   ComponentEnable(DecodeBufferComponentType_t  Component,
                                                  bool                         Enabled);

    //
    // A stream can reserve some memory area (manifestor display zone) using the
    // ReserveMemory() method: after the stream has closed, its reserved memory should not be drawn over
    // by a new stream (at least at the beginning of the new stream) for aesthetic reason.
    // The ArchiveReservedMemory() method is in charge of archiving into the Blocks argument
    // the necessary data for the new stream to avoid writing in the reserved zone. This method should be called
    // when a stream closes.
    //
    // Each block element of Blocks corresponds to a stream reserved memory data. Each time the function is called
    // the first unused block is filled out.
    // If the array is full then DecodeBufferManagerError is returned. So before calling this method
    // at least one block element should be unused (the caller is in charge of freeing a block element).
    //
    // Note that if more than  MAXIMUM_NUMBER_OF_RESERVED_MANIFESTATION_MEMORY_BLOCKS streams reserve memory
    // then some memory area could be drawn over.
    //
    //
    DecodeBufferManagerStatus_t   ArchiveReservedMemory(DecodeBufferReservedBlock_t Blocks[MAXIMUM_NUMBER_OF_RESERVED_MANIFESTATION_MEMORY_BLOCKS]);


    //
    // See ArchiveReservedMemory comments for a general comprehension of the reserved memory
    // mechanism.
    // This method is in charge of initializing (resetting in fact) the current reserved memory
    // of the stream. It also displays a message if a component has been allocated in a reserved
    // area. This can occur if more than  MAXIMUM_NUMBER_OF_RESERVED_MANIFESTATION_MEMORY_BLOCKS streams
    // have reserved memory
    //
    //
    DecodeBufferManagerStatus_t   InitializeReservedMemory(DecodeBufferReservedBlock_t  Blocks[MAXIMUM_NUMBER_OF_RESERVED_MANIFESTATION_MEMORY_BLOCKS]);

    //
    // See ArchiveReservedMemory comments for a general comprehension of the reserved memory
    // mechanism.
    // This method is in charge of reserving memory for the current stream.
    //
    DecodeBufferManagerStatus_t   ReserveMemory(void *Base, unsigned int Size);

    DecodeBufferManagerStatus_t   AbortGetBuffer(void);
    //
    // This function displays some content of the components.
    //
    void DisplayComponents();

    //
    // Functions to get/release a decode buffer, or to free up some of the components when they are no longer needed
    //

    DecodeBufferManagerStatus_t   GetDecodeBuffer(DecodeBufferRequest_t   *Request,
                                                  Buffer_t                *Buffer);
    DecodeBufferManagerStatus_t   EnsureReferenceComponentsPresent(
        Buffer_t                 Buffer);
    DecodeBufferManagerStatus_t   GetEstimatedBufferCount(unsigned int         *Count,
                                                          DecodeBufferUsage_t   For = ForManifestation);
    DecodeBufferManagerStatus_t   ReleaseBuffer(Buffer_t Buffer,
                                                bool     ForReference);
    DecodeBufferManagerStatus_t   ReleaseBufferComponents(Buffer_t              Buffer,
                                                          DecodeBufferUsage_t   ForUse);

    DecodeBufferManagerStatus_t   IncrementReferenceUseCount(Buffer_t Buffer);
    DecodeBufferManagerStatus_t   DecrementReferenceUseCount(Buffer_t Buffer);

    //
    // Accessors - in order to access pointers etc
    //

    BufferFormat_t   ComponentDataType(DecodeBufferComponentType_t  Component);
    unsigned int     ComponentBorder(DecodeBufferComponentType_t    Component,
                                     unsigned int                   Index);
    void            *ComponentAddress(DecodeBufferComponentType_t   Component,
                                      AddressType_t                 AlternateAddressType);

    bool             ComponentPresent(Buffer_t                    Buffer,
                                      DecodeBufferComponentType_t Component);

    unsigned int     ComponentSize(Buffer_t         Buffer,
                                   DecodeBufferComponentType_t Component);
    void            *ComponentBaseAddress(Buffer_t                    Buffer,
                                          DecodeBufferComponentType_t Component);
    void            *ComponentBaseAddress(Buffer_t                    Buffer,
                                          DecodeBufferComponentType_t Component,
                                          AddressType_t               AlternateAddressType);

    unsigned int     ComponentDimension(Buffer_t                    Buffer,
                                        DecodeBufferComponentType_t Component,
                                        unsigned int                DimensionIndex);
    unsigned int     ComponentStride(Buffer_t                    Buffer,
                                     DecodeBufferComponentType_t Component,
                                     unsigned int                DimensionIndex,
                                     unsigned int               ComponentIndex);

    unsigned int     DecimationFactor(Buffer_t     Buffer,
                                      unsigned int DecimationIndex);


    unsigned int     LumaSize(Buffer_t                    Buffer,
                              DecodeBufferComponentType_t Component);
    unsigned char   *Luma(Buffer_t                    Buffer,
                          DecodeBufferComponentType_t Component);
    unsigned char   *Luma(Buffer_t                    Buffer,
                          DecodeBufferComponentType_t Component,
                          AddressType_t               AlternateAddressType);
    unsigned char   *LumaInsideBorder(Buffer_t                    Buffer,
                                      DecodeBufferComponentType_t Component);
    unsigned char   *LumaInsideBorder(Buffer_t                    Buffer,
                                      DecodeBufferComponentType_t Component,
                                      AddressType_t               AlternateAddressType);

    unsigned int     ChromaSize(Buffer_t                    Buffer,
                                DecodeBufferComponentType_t Component);
    unsigned char   *Chroma(Buffer_t                    Buffer,
                            DecodeBufferComponentType_t Component);
    unsigned char   *Chroma(Buffer_t                    Buffer,
                            DecodeBufferComponentType_t Component,
                            AddressType_t               AlternateAddressType);
    unsigned char   *ChromaInsideBorder(Buffer_t                    Buffer,
                                        DecodeBufferComponentType_t Component);
    unsigned char   *ChromaInsideBorder(Buffer_t                    Buffer,
                                        DecodeBufferComponentType_t Component,
                                        AddressType_t               AlternateAddressType);

    unsigned int     RasterSize(Buffer_t                    Buffer,
                                DecodeBufferComponentType_t Component);
    unsigned char   *Raster(Buffer_t                    Buffer,
                            DecodeBufferComponentType_t Component);
    unsigned char   *Raster(Buffer_t                    Buffer,
                            DecodeBufferComponentType_t Component,
                            AddressType_t               AlternateAddressType);

protected:
    OS_Mutex_t                   Lock;
    BufferManager_t              BufferManager;

    unsigned int                 NumberOfDecodeBuffers;
    BufferPool_t                 DecodeBufferPool;

    bool                         StreamSwitchOccurring;

    DecodeBufferComponentData_t  ComponentData[NUMBER_OF_COMPONENTS];
    DecodeBufferComponentData_t  StreamSwitchcopyOfComponentData[NUMBER_OF_COMPONENTS];

    unsigned char               *ReservedMemoryBase;
    unsigned int                 ReservedMemorySize;

    DecodeBufferManagerStatus_t  CreateDecodeBufferPool(void);
    DecodeBufferManagerStatus_t  CreateComponentPool(unsigned int Component);
    DecodeBufferManagerStatus_t  CheckForSwitchStreamTermination(void);

    DecodeBufferManagerStatus_t  AllocateComponent(unsigned int                 Component,
                                                   DecodeBufferContextRecord_t *BufferContext);
    DecodeBufferManagerStatus_t  FillOutComponentStructure(unsigned int                 Component,
                                                           DecodeBufferContextRecord_t *BufferContext);

    DecodeBufferContextRecord_t  *GetBufferContext(Buffer_t Buffer);

private:
    DISALLOW_COPY_AND_ASSIGN(DecodeBufferManager_Base_c);
};

#endif
