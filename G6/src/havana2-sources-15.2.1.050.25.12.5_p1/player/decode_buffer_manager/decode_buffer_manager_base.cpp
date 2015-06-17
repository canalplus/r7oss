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

#include "decode_buffer_manager_base.h"

#undef TRACE_TAG
#define TRACE_TAG "DecodeBufferManager_Base_c"

#define MAX_BUFFER_RETRIES      2

// -----

#define BUFFER_DECODE_BUFFER        "DecodeBufferContext"
#define BUFFER_DECODE_BUFFER_TYPE   {BUFFER_DECODE_BUFFER, BufferDataTypeBase, AllocateFromOSMemory, 8, 0, true, true, sizeof(DecodeBufferContextRecord_t)}

static BufferDataDescriptor_t       InitialDecodeBufferContextDescriptor = BUFFER_DECODE_BUFFER_TYPE;

// -----
#define BUFFER_INDIVIDUAL_DECODE_BUFFER         "IndividualDecodeBuffer"
#define BUFFER_INDIVIDUAL_DECODE_BUFFER_TYPE        {BUFFER_INDIVIDUAL_DECODE_BUFFER, BufferDataTypeBase, AllocateIndividualSuppliedBlocks,1024,1024, true, true, DECODE_BUFFER_HD_SIZE}
static BufferDataDescriptor_t               IndividualBufferDescriptor = BUFFER_INDIVIDUAL_DECODE_BUFFER_TYPE;


#define BUFFER_SIMPLE_DECODE_BUFFER         "SimpleDecodeBuffer"
#define BUFFER_SIMPLE_DECODE_BUFFER_TYPE        {BUFFER_SIMPLE_DECODE_BUFFER, BufferDataTypeBase, AllocateFromSuppliedBlock, 1024, 1024, false, false, 0}
static BufferDataDescriptor_t               InitialSimpleBufferDescriptor = BUFFER_SIMPLE_DECODE_BUFFER_TYPE;

#define BUFFER_MACROBLOCK_STRUCTURE_BUFFER      "MacroBlockStructureBuffer"
#define BUFFER_MACROBLOCK_STRUCTURE_BUFFER_TYPE     {BUFFER_MACROBLOCK_STRUCTURE_BUFFER, BufferDataTypeBase, AllocateFromSuppliedBlock, 256, 256,  false, false, 0}

static BufferDataDescriptor_t           InitialMacroBlockStructureBufferDescriptor = BUFFER_MACROBLOCK_STRUCTURE_BUFFER_TYPE;

#define BUFFER_VIDEO_POST_PROCESSING_CONTROL            "VideoPostProcessingControl"
#define BUFFER_VIDEO_POST_PROCESSING_CONTROL_TYPE       {BUFFER_VIDEO_POST_PROCESSING_CONTROL, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(VideoPostProcessingControl_t)}

static BufferDataDescriptor_t       InitialVideoPostProcessBufferDescriptor = BUFFER_VIDEO_POST_PROCESSING_CONTROL_TYPE;

#define BUFFER_ADDITIONAL_BLOCK                 "AdditionalBlock"
#define BUFFER_ADDITIONAL_BLOCK_TYPE                {BUFFER_ADDITIONAL_BLOCK, BufferDataTypeBase, AllocateFromSuppliedBlock, 32, 0, false, false, 0}

static BufferDataDescriptor_t           InitialAdditionalBlockDescriptor = BUFFER_ADDITIONAL_BLOCK_TYPE;


// ---------------------------------------------------------------------
//
//      The Constructor function
//
DecodeBufferManager_Base_c::DecodeBufferManager_Base_c(void)
    : Lock()
    , BufferManager(NULL)
    , NumberOfDecodeBuffers(0)
    , DecodeBufferPool(NULL)
    , StreamSwitchOccurring(false)
    , ComponentData()
    , StreamSwitchcopyOfComponentData()
    , ReservedMemoryBase(NULL)
    , ReservedMemorySize(0)
{
    OS_InitializeMutex(&Lock);

    unsigned int i;
    for (i = 0; i < NUMBER_OF_COMPONENTS; i++)
    {
        ComponentData[i].AllocatorMemoryDevice  = NULL;
        ComponentData[i].PostponeReset = false;
    }
}

// ---------------------------------------------------------------------
//
//      The Destructor function
//

DecodeBufferManager_Base_c::~DecodeBufferManager_Base_c(void)
{
    DecodeBufferManager_Base_c::Halt();

    DecodeBufferManager_Base_c::Reset();

    OS_TerminateMutex(&Lock);
}


// ---------------------------------------------------------------------
//
//      The Halt function, give up access to any registered resources
//

DecodeBufferManagerStatus_t   DecodeBufferManager_Base_c::Halt(void)
{
    //
    // We have no access other component parts in this class, so halt
    // has no function other than that expressed by the base class.
    BaseComponentClass_c::Halt();
    return DecodeBufferManagerNoError;
}

// ---------------------------------------------------------------------
// to call ics_region_remove at CPS entry

void   DecodeBufferManager_Base_c::ResetDecoderMap(void)
{
    SE_DEBUG(group_decodebufferm, "Reset Ics map ");
    OS_LockMutex(&Lock);

    for (int i = 0; i < NUMBER_OF_COMPONENTS; i++)
    {
        if (ComponentData[i].AllocatorMemoryDevice != NULL)
        {
            AllocatorRemoveMapEx(ComponentData[i].AllocatorMemoryDevice->UnderlyingDevice);
        }
    }
    OS_UnLockMutex(&Lock);
}


DecodeBufferManagerStatus_t   DecodeBufferManager_Base_c::CreateDecoderMap(void)
{
    DecodeBufferManagerStatus_t      Status = DecodeBufferManagerNoError;
    allocator_status_t       AStatus = allocator_ok;

    SE_DEBUG(group_decodebufferm, "Create Ics map ");
    OS_LockMutex(&Lock);

    for (int i = 0; i < NUMBER_OF_COMPONENTS; i++)
    {
        if (ComponentData[i].AllocatorMemoryDevice != NULL)
        {
            AStatus = AllocatorCreateMapEx(ComponentData[i].AllocatorMemoryDevice->UnderlyingDevice);
            if (AStatus == allocator_error)
            {
                Status = DecodeBufferManagerError;
                break;
            }
        }
    }
    OS_UnLockMutex(&Lock);
    return Status;
}
// ---------------------------------------------------------------------
// reset for stream switch

DecodeBufferManagerStatus_t   DecodeBufferManager_Base_c::Reset(void)
{
    unsigned int    i;
    SE_DEBUG(group_decodebufferm, ">\n");

    //
    // First we release any pools held over during a stream switch
    //

    OS_LockMutex(&Lock);

    if (StreamSwitchOccurring)
    {
        StreamSwitchOccurring   = false;

        for (i = 0; i < NUMBER_OF_COMPONENTS; i++)
            if (StreamSwitchcopyOfComponentData[i].BufferPool != NULL)
            {
                BufferManager->DestroyPool(StreamSwitchcopyOfComponentData[i].BufferPool);

                AllocatorClose(&StreamSwitchcopyOfComponentData[i].AllocatorMemoryDevice);
            }
    }

    memset(&StreamSwitchcopyOfComponentData, 0, NUMBER_OF_COMPONENTS * sizeof(DecodeBufferComponentData_t));

    //
    // Now the standard list
    //

    for (i = 0; i < NUMBER_OF_COMPONENTS; i++)
        if (ComponentData[i].BufferPool != NULL)
        {
            // If a component has been tagged as "PostponeReset" it means that a part of its
            // memory has been reserved so we do not destroy the pool right now. It prevents
            // a new stream to allocate memory that overlaps with the reserved one.
            // When a component is tagged as "PostponeReset" its buffer pool and allocator
            // memory device has been archived by DecodeBufferManager_Base_c::ArchiveReservedMemory(Blocks)
            // and the caller of this function is in charge of destroying the pool and closing the
            // allocator memory device archived in Blocks.
            if (ComponentData[i].PostponeReset)
            {
                SE_VERBOSE(group_decodebufferm, "@ Reset of component %d is postponed. BufferPool=%p, AllocatorMemoryDevice=%p\n",
                           i, (void *)(ComponentData[i].BufferPool), (void *)(ComponentData[i].AllocatorMemoryDevice));
            }
            else
            {
                BufferManager->DestroyPool(ComponentData[i].BufferPool);

                AllocatorClose(&ComponentData[i].AllocatorMemoryDevice);
            }
        }

    memset(&ComponentData, 0, NUMBER_OF_COMPONENTS * sizeof(DecodeBufferComponentData_t));

    //
    // Finally the buffer pool itself needs to go
    //

    if (DecodeBufferPool != NULL)
    {
        BufferManager->DestroyPool(DecodeBufferPool);
        DecodeBufferPool = NULL;
    }

    OS_UnLockMutex(&Lock);

    SE_DEBUG(group_decodebufferm, "<\n");

    return BaseComponentClass_c::Reset();
}


// ---------------------------------------------------------------------
//
//      Override for component base class set module parameters function
//

DecodeBufferManagerStatus_t   DecodeBufferManager_Base_c::SetModuleParameters(
    unsigned int              ParameterBlockSize,
    void                     *ParameterBlock)
{
    DecodeBufferManagerParameterBlock_t *DecodeBufferManagerParameterBlock = (DecodeBufferManagerParameterBlock_t *)ParameterBlock;
    DecodeBufferManagerStatus_t      Status;

    if (ParameterBlockSize != sizeof(DecodeBufferManagerParameterBlock_t))
    {
        SE_ERROR("Invalid parameter block\n");
        return DecodeBufferManagerError;
    }

    Status  = DecodeBufferManagerNoError;

    switch (DecodeBufferManagerParameterBlock->ParameterType)
    {
    case DecodeBufferManagerPartitionData:
        //
        // Switch the parameters - assert that we are currently in a reset state
        //
        AssertComponentState(ComponentReset);
        NumberOfDecodeBuffers       = DecodeBufferManagerParameterBlock->PartitionData.NumberOfDecodeBuffers;
        ComponentData[PrimaryManifestationComponent].AllocationPartitionSize    = DecodeBufferManagerParameterBlock->PartitionData.PrimaryManifestationPartitionSize;
        memcpy(&ComponentData[PrimaryManifestationComponent].AllocationPartitionName, &DecodeBufferManagerParameterBlock->PartitionData.PrimaryManifestationPartitionName, ALLOCATOR_MAX_PARTITION_NAME_SIZE);
        ComponentData[DecimatedManifestationComponent].AllocationPartitionSize  = DecodeBufferManagerParameterBlock->PartitionData.DecimatedManifestationPartitionSize;
        memcpy(&ComponentData[DecimatedManifestationComponent].AllocationPartitionName, &DecodeBufferManagerParameterBlock->PartitionData.DecimatedManifestationPartitionName,
               ALLOCATOR_MAX_PARTITION_NAME_SIZE);
        ComponentData[VideoDecodeCopy].AllocationPartitionSize  = DecodeBufferManagerParameterBlock->PartitionData.VideoDecodeCopyPartitionSize;
        memcpy(&ComponentData[VideoDecodeCopy].AllocationPartitionName, &DecodeBufferManagerParameterBlock->PartitionData.VideoDecodeCopyPartitionName, ALLOCATOR_MAX_PARTITION_NAME_SIZE);
        ComponentData[VideoMacroblockStructure].AllocationPartitionSize = DecodeBufferManagerParameterBlock->PartitionData.VideoMacroblockStructurePartitionSize;
        memcpy(&ComponentData[VideoMacroblockStructure].AllocationPartitionName, &DecodeBufferManagerParameterBlock->PartitionData.VideoMacroblockStructurePartitionName, ALLOCATOR_MAX_PARTITION_NAME_SIZE);
        ComponentData[VideoPostProcessControl].AllocationPartitionSize  = DecodeBufferManagerParameterBlock->PartitionData.VideoPostProcessControlPartitionSize;
        memcpy(&ComponentData[VideoPostProcessControl].AllocationPartitionName, &DecodeBufferManagerParameterBlock->PartitionData.VideoPostProcessControlPartitionName, ALLOCATOR_MAX_PARTITION_NAME_SIZE);
        ComponentData[AdditionalMemoryBlock].AllocationPartitionSize    = DecodeBufferManagerParameterBlock->PartitionData.AdditionalMemoryBlockPartitionSize;
        memcpy(&ComponentData[AdditionalMemoryBlock].AllocationPartitionName, &DecodeBufferManagerParameterBlock->PartitionData.AdditionalMemoryBlockPartitionName, ALLOCATOR_MAX_PARTITION_NAME_SIZE);
        break;

    case DecodeBufferManagerPrimaryIndividualBufferList:
        //
        // Save the 'NumberOfDecodeBuffers' provided individual buffer
        //
        NumberOfDecodeBuffers = DecodeBufferManagerParameterBlock->PrimaryIndividualData.NumberOfBufs;
        strncpy(ComponentData[PrimaryManifestationComponent].AllocationPartitionName, IndividualBuffersPartition,
                sizeof(ComponentData[PrimaryManifestationComponent].AllocationPartitionName));
        ComponentData[PrimaryManifestationComponent].AllocationPartitionName[
            sizeof(ComponentData[PrimaryManifestationComponent].AllocationPartitionName) - 1] = '\0';
        ComponentData[PrimaryManifestationComponent].IndividualBufferSize = DecodeBufferManagerParameterBlock->PrimaryIndividualData.BufSize;
        memcpy(ComponentData[PrimaryManifestationComponent].ArrayOfMemoryBlocks,
               DecodeBufferManagerParameterBlock->PrimaryIndividualData.AllocatorMemory,
               sizeof(DecodeBufferManagerParameterBlock->PrimaryIndividualData.AllocatorMemory));
        break;

    default:
        SE_ERROR("Unrecognised parameter block %d\n",
                 DecodeBufferManagerParameterBlock->ParameterType);
        Status  = DecodeBufferManagerError;
    }

    return  Status;
}


// ---------------------------------------------------------------------
//
//      Function to return the decode buffer pool to the caller.
//

DecodeBufferManagerStatus_t   DecodeBufferManager_Base_c::GetDecodeBufferPool(BufferPool_t  *Pool)
{
    DecodeBufferManagerStatus_t      Status;
//
    OS_LockMutex(&Lock);

    if (DecodeBufferPool == NULL)
    {
        Status  = CreateDecodeBufferPool();

        if (Status != DecodeBufferManagerNoError)
        {
            OS_UnLockMutex(&Lock);
            return Status;
        }
    }

    OS_UnLockMutex(&Lock);
    *Pool   = DecodeBufferPool;
    return  DecodeBufferManagerNoError;
}


// ---------------------------------------------------------------------
//
//      Function to support entry into stream switch mode
//

DecodeBufferManagerStatus_t   DecodeBufferManager_Base_c::EnterStreamSwitch(void)
{
    if (StreamSwitchOccurring)
    {
        SE_FATAL("Still in last stream switch\n");
    }

    StreamSwitchOccurring   = true;
    return  DecodeBufferManagerNoError;
}


// ---------------------------------------------------------------------
//
//      Function to inform the class of the components that make
//  up a decode buffer.
//

DecodeBufferManagerStatus_t   DecodeBufferManager_Base_c::FillOutDefaultList(
    BufferFormat_t               PrimaryFormat,
    DecodeBufferComponentElementMask_t   Elements,
    DecodeBufferComponentDescriptor_t    List[])
{
    unsigned int  Count   = 0;

    if ((Elements & PrimaryManifestationElement) != 0)
    {
        memset(&List[Count], 0, sizeof(DecodeBufferComponentDescriptor_t));
        List[Count].Type        = PrimaryManifestationComponent;
        List[Count].Usage       = ((Elements & VideoDecodeCopyElement) != 0) ?
                                  ForManifestation :
                                  (ForManifestation | ForReference);
        List[Count].DataType        = PrimaryFormat;
        List[Count].DefaultAddressMode  = (PrimaryFormat == FormatAudio) ? CachedAddress : PhysicalAddress;
        List[Count].DataDescriptor  = &InitialSimpleBufferDescriptor;

        //
        // When partition name is "AllocatedLaterPartition", set Component usage as "notUsed" to avoid useless allocation
        // When partition name is "IndividualBuffersPartition", set Component whit the individual buffers list
        //
        if (strcmp(ComponentData[PrimaryManifestationComponent].AllocationPartitionName, AllocatedLaterPartition) == 0)
        {
            List[Count].Usage = NotUsed;
        }
        else if (strcmp(ComponentData[PrimaryManifestationComponent].AllocationPartitionName, IndividualBuffersPartition) == 0)
        {
            List[Count].DataDescriptor = &IndividualBufferDescriptor;
        }

        Count++;
    }

    if ((Elements & DecimatedManifestationElement) != 0)
    {
        memset(&List[Count], 0, sizeof(DecodeBufferComponentDescriptor_t));
        List[Count].Type        = DecimatedManifestationComponent;
        List[Count].Usage       = ForManifestation;
        List[Count].DataType        = PrimaryFormat;
        List[Count].DefaultAddressMode  = PhysicalAddress;
        List[Count].DataDescriptor  = &InitialSimpleBufferDescriptor;
        Count++;
    }

    if ((Elements & VideoDecodeCopyElement) != 0)
    {
        memset(&List[Count], 0, sizeof(DecodeBufferComponentDescriptor_t));
        List[Count].Type        = VideoDecodeCopy;
        List[Count].Usage       = ForReference | ForDecode;
        List[Count].DataType        = FormatVideo420_PairedMacroBlock;
        List[Count].DefaultAddressMode  = PhysicalAddress;
        List[Count].DataDescriptor  = &InitialSimpleBufferDescriptor;
        Count++;
    }

    if ((Elements & VideoMacroblockStructureElement) != 0)
    {
        memset(&List[Count], 0, sizeof(DecodeBufferComponentDescriptor_t));
        List[Count].Type        = VideoMacroblockStructure;
        List[Count].Usage       = ForReference;
        List[Count].DataType        = FormatPerMacroBlockLinear;
        List[Count].DefaultAddressMode  = PhysicalAddress;
        List[Count].DataDescriptor  = &InitialMacroBlockStructureBufferDescriptor;
        Count++;
    }

    if ((Elements & VideoPostProcessControlElement) != 0)
    {
        memset(&List[Count], 0, sizeof(DecodeBufferComponentDescriptor_t));
        List[Count].Type        = VideoPostProcessControl;
        List[Count].Usage       = ForManifestation;
        List[Count].DataType        = FormatLinear;
        List[Count].DefaultAddressMode  = CachedAddress;
        List[Count].DataDescriptor  = &InitialVideoPostProcessBufferDescriptor;
        Count++;
    }

    if ((Elements & AdditionalMemoryBlockElement) != 0)
    {
        memset(&List[Count], 0, sizeof(DecodeBufferComponentDescriptor_t));
        List[Count].Type        = AdditionalMemoryBlock;
        List[Count].Usage       = ForManifestation | ForReference;
        List[Count].DataType        = FormatLinear;
        List[Count].DefaultAddressMode  = PhysicalAddress;
        List[Count].DataDescriptor  = &InitialAdditionalBlockDescriptor;
        Count++;
    }

    List[Count].Type            = UndefinedComponent;
    return DecodeBufferManagerNoError;
}


// ---------------------------------------------------------------------
//
//      Function to inform the class of the components that make
//  up a decode buffer.
//

DecodeBufferManagerStatus_t   DecodeBufferManager_Base_c::InitializeComponentList(DecodeBufferComponentDescriptor_t *List)
{
    unsigned int            i;
    DecodeBufferManagerStatus_t Status;

    //
    // check we have a filled in list.
    //

    if (List[0].Type >= UndefinedComponent)
    {
        SE_FATAL("Bad component list %02x\n", List[0].Type);
    }

    //
    // If we are in stream switching, copy the components, and
    // see which ones get re-used, attempt deletions later.
    //

    if (StreamSwitchOccurring)
    {
        memcpy(StreamSwitchcopyOfComponentData, ComponentData, NUMBER_OF_COMPONENTS * sizeof(DecodeBufferComponentData_t));

        for (i = 0; i < NUMBER_OF_COMPONENTS; i++)
        {
            memset(&ComponentData[i].ComponentDescriptor, 0, sizeof(DecodeBufferComponentDescriptor_t));
            ComponentData[i].Enabled            = false;
            ComponentData[i].AllocatorMemoryDevice  = NULL;
            ComponentData[i].BufferPool         = NULL;
            ComponentData[i].PostponeReset = false;
        }
    }

    //
    // Scan the list and create the appropriate pools
    //

    for (i = 0; List[i].Type < UndefinedComponent; i++)
    {
        memcpy(&ComponentData[List[i].Type].ComponentDescriptor, &List[i], sizeof(DecodeBufferComponentDescriptor_t));
    }

    for (i = 0; i < NUMBER_OF_COMPONENTS; i++)
        if (ComponentData[i].ComponentDescriptor.Usage != NotUsed)
        {
            Status  = CreateComponentPool(i);

            if (Status != DecodeBufferManagerNoError)
            {
                SE_ERROR("Failed to create component pools for decode buffers\n");
                Reset();
                return DecodeBufferManagerError;
            }

            ComponentData[i].Enabled    = true;     // Default enable a component
            ComponentData[i].PostponeReset = false;
        }

    //
    // Before returning, we check if in switch stream mode, can we let anything go
    //

    if (StreamSwitchOccurring)
    {
        CheckForSwitchStreamTermination();
    }

    return DecodeBufferManagerNoError;
}


// ---------------------------------------------------------------------
//
//      Simplifying function to allow those that just want a primary
//  with a varying buffer type.
//

DecodeBufferManagerStatus_t   DecodeBufferManager_Base_c::InitializeSimpleComponentList(BufferFormat_t      Type)
{
    DecodeBufferComponentDescriptor_t   List[2];
    FillOutDefaultList(Type, PrimaryManifestationElement, List);
    return  InitializeComponentList(List);
}


// ---------------------------------------------------------------------
//
//      Functions to switch the format of the output (used in theora)
//

DecodeBufferManagerStatus_t   DecodeBufferManager_Base_c::ModifyComponentFormat(
    DecodeBufferComponentType_t      Component,
    BufferFormat_t               NewFormat)
{
    ComponentData[Component].ComponentDescriptor.DataType   = NewFormat;
    return DecodeBufferManagerNoError;
}


// ---------------------------------------------------------------------
//
//      Functions to enable/disable the generation of a particular component
//

DecodeBufferManagerStatus_t   DecodeBufferManager_Base_c::ComponentEnable(
    DecodeBufferComponentType_t      Component,
    bool                     Enabled)
{
    ComponentData[Component].Enabled    = Enabled;
    return DecodeBufferManagerNoError;
}


// ---------------------------------------------------------------------
//
//      Functions to support the reservation of memory across playbacks
//

DecodeBufferManagerStatus_t   DecodeBufferManager_Base_c::ArchiveReservedMemory(DecodeBufferReservedBlock_t Blocks[MAXIMUM_NUMBER_OF_RESERVED_MANIFESTATION_MEMORY_BLOCKS])
{
    DecodeBufferManagerStatus_t returnValue = DecodeBufferManagerNoError;
    int componentThatReserves = -1;
    SE_DEBUG(group_decodebufferm, ">\n");

    // Do something only if memory has been reserved.
    if (ReservedMemoryBase != NULL)
    {
        // Get the component that corresponds to ReservedMemoryBase
        for (int j = 0; j < NUMBER_OF_COMPONENTS; j++)
        {
            if (ComponentData[j].AllocationPartitionSize != 0)
            {
                unsigned char *DataStart = (unsigned char *)ComponentData[j].AllocatorMemory[PhysicalAddress];
                unsigned char *DataEnd = DataStart + ComponentData[j].AllocationPartitionSize;
                SE_VERBOSE(group_decodebufferm, "@ DataStart=%p, DataEnd=%p, ReservedMemoryBase=%p, ReservedMemoryEnd = %p\n",
                           DataStart, DataEnd, ReservedMemoryBase, ReservedMemoryBase + ReservedMemorySize);

                if (DataStart <= ReservedMemoryBase && ReservedMemoryBase <= DataEnd)
                {
                    componentThatReserves = j;
                    SE_VERBOSE(group_decodebufferm, "@ Component %d reserves memory\n", componentThatReserves);
                }
            }
        }

        // If no component has been found that corresponds to the reserved memory then return an error.
        if (componentThatReserves == -1)
        {
            SE_ERROR("@ There is a reserved memory base but no component pool associated has been found\n");
            returnValue = DecodeBufferManagerError;
        }
        // If we have found the corresponding component pool.
        else
        {
            // Check whether the archive is full or not.
            bool archiveIsFull = true;

            for (int i = 0; i < MAXIMUM_NUMBER_OF_RESERVED_MANIFESTATION_MEMORY_BLOCKS; i++)
            {
                if (Blocks[i].BlockSize == 0)
                {
                    archiveIsFull = false;
                }
            }

            // If archive is full we return an error. The caller of this function is indeed in charge
            // of freeing blocks before calling it.
            if (archiveIsFull)
            {
                SE_ERROR("@ Archive is full, please delete the oldest archive block\n");
                returnValue = DecodeBufferManagerError;
            }
            else
            {
                bool archiveDone = false;

                for (int i = 0; i < MAXIMUM_NUMBER_OF_RESERVED_MANIFESTATION_MEMORY_BLOCKS && !archiveDone; i++)
                {
                    if (Blocks[i].BlockSize == 0)
                    {
                        Blocks[i].BlockBase = ReservedMemoryBase;
                        Blocks[i].BlockSize = ReservedMemorySize;
                        SE_VERBOSE(group_decodebufferm, " @ Archiving in block %d ComponentData[%d].BufferPool=%p and ComponentData[%d].AllocatorMemoryDevice=%p\n", i,
                                   componentThatReserves,
                                   (void *)(ComponentData[componentThatReserves].BufferPool),
                                   componentThatReserves,
                                   (void *)(ComponentData[componentThatReserves].AllocatorMemoryDevice));
                        // Tag the component as "PostponeReset": this will prevent the buffer pool and
                        // the allocator memory device of the component to be released when
                        // DecodeBufferManager_Base_c::Reset() will be called. The responsibility of
                        // releasing these buffer pool and allocator is now in the caller of this function.
                        ComponentData[componentThatReserves].PostponeReset = true;
                        Blocks[i].BufferPool = ComponentData[componentThatReserves].BufferPool;
                        Blocks[i].AllocatorMemoryDevice = ComponentData[componentThatReserves].AllocatorMemoryDevice;
                        archiveDone = true; // Exit the loop.
                        returnValue = DecodeBufferManagerNoError;
                    }
                }
            }
        }
    }

    SE_DEBUG(group_decodebufferm, "<\n");
    return returnValue;
}

// ---------------------------------------------------------------------
//
// It displays the content of the components.
//

void DecodeBufferManager_Base_c::DisplayComponents()
{
    SE_DEBUG(group_decodebufferm, ">\n");

    for (int j = 0; j < NUMBER_OF_COMPONENTS; j++)
    {
        unsigned char *DataStart = (unsigned char *)ComponentData[j].AllocatorMemory[PhysicalAddress];
        unsigned char *DataEnd = DataStart + ComponentData[j].AllocationPartitionSize;
        SE_DEBUG(group_decodebufferm, "@ Component: %d, DataStart=%p, DataEnd=%p, ComponentData[%d].BufferPool=%p, ComponentData[%d].AllocatorMemoryDevice=%p\n", j, DataStart, DataEnd, j,
                 (void *)(ComponentData[j].BufferPool), j, (void *)(ComponentData[j].AllocatorMemoryDevice));
    }

    SE_DEBUG(group_decodebufferm, "<\n");
}

// ---------------------------------------------------------------------
//
// It initializes the reserve memory of the current stream by resetting it.
//

DecodeBufferManagerStatus_t   DecodeBufferManager_Base_c::InitializeReservedMemory(DecodeBufferReservedBlock_t  Blocks[MAXIMUM_NUMBER_OF_RESERVED_MANIFESTATION_MEMORY_BLOCKS])
{
    unsigned int    i, j;
    unsigned char   *DataStart;
    unsigned char   *DataEnd;
    unsigned char   *ReservedStart;
    unsigned char   *ReservedEnd;
    bool         Reserved;
    SE_DEBUG(group_decodebufferm, ">\n");
    ReservedMemoryBase  = NULL;
    ReservedMemorySize  = 0;

    for (i = 0; i < MAXIMUM_NUMBER_OF_RESERVED_MANIFESTATION_MEMORY_BLOCKS; i++)
        if (Blocks[i].BlockSize != 0)
            for (j = 0; j < NUMBER_OF_COMPONENTS; j++)
                if (ComponentData[j].AllocationPartitionSize != 0)
                {
                    DataStart           = (unsigned char *)ComponentData[j].AllocatorMemory[PhysicalAddress];
                    DataEnd         = DataStart + ComponentData[j].AllocationPartitionSize;
                    ReservedStart       = Blocks[i].BlockBase;
                    ReservedEnd         = ReservedStart + Blocks[i].BlockSize;
                    Reserved            = (DataStart < ReservedStart) ?
                                          (DataEnd > ReservedStart) :
                                          (DataStart < ReservedEnd);

                    if (Reserved)
                    {
                        SE_WARNING("*** Caution: a reserved memory area can be drawn over***\n");
                    }
                }

    SE_DEBUG(group_decodebufferm, "<\n");
    return DecodeBufferManagerNoError;
}


// ---------------------------------------------------------------------
//
//      Functions to reserve a block of memory (used by manifestor to
//  hold screen memory even after releasing buffer.
//

DecodeBufferManagerStatus_t   DecodeBufferManager_Base_c::ReserveMemory(
    void                    *Base,
    unsigned int                 Size)
{
    ReservedMemoryBase  = (unsigned char *)Base;
    ReservedMemorySize  = Size;
    return DecodeBufferManagerNoError;
}

// ---------------------------------------------------------------------
//
// Function to abort GetBuffer() action from DecodeBufferPool
// TODO(pht) use with care.. might be better to remove it
//

DecodeBufferManagerStatus_t  DecodeBufferManager_Base_c::AbortGetBuffer(void)
{
    DecodeBufferPool->AbortBlockingGetBuffer();
    return DecodeBufferManagerNoError;
}

// ---------------------------------------------------------------------
//
//      Functions to get a buffer and allocate all the appropriate components
//

DecodeBufferManagerStatus_t   DecodeBufferManager_Base_c::GetDecodeBuffer(
    DecodeBufferRequest_t   *Request,
    Buffer_t        *Buffer)
{
    unsigned int                 i;
    Buffer_t                     NewDecodeBuffer;
    DecodeBufferContextRecord_t *BufferContext;
    bool                         NeedToAllocateComponent;
    DecodeBufferUsage_t          NeedComponentsFor;

    *Buffer = NULL;

    //
    // Since people can release without calling us, we check for any stream switch being over
    //

    if (StreamSwitchOccurring)
    {
        CheckForSwitchStreamTermination();
    }

    //
    // Get me a new decode buffer
    //
    BufferStatus_t BufferStatus = DecodeBufferPool->GetBuffer(&NewDecodeBuffer);
    if (BufferStatus != BufferNoError)
    {
        SE_ERROR("Failed to allocate a decode buffer\n");
        return DecodeBufferManagerError;
    }

    //
    // Initialize the context record
    //
    NewDecodeBuffer->ObtainDataReference(NULL, NULL, (void **)(&BufferContext));
    SE_ASSERT(BufferContext != NULL); // cannot be empty
    memset(BufferContext, 0, sizeof(DecodeBufferContextRecord_t));
    BufferContext->DecodeBuffer     = NewDecodeBuffer;
    memcpy(&BufferContext->Request, Request, sizeof(DecodeBufferRequest_t));
    //
    // allocate the components - note no components for a marker frame.
    //
    NeedComponentsFor       = ForDecode     |
                              ForManifestation  |
                              (Request->ReferenceFrame ? ForReference : NotUsed);

    for (i = 0; i < NUMBER_OF_COMPONENTS; i++)
    {
        NeedToAllocateComponent = !Request->MarkerFrame     &&
                                  ComponentData[i].Enabled  &&
                                  ((ComponentData[i].ComponentDescriptor.Usage & NeedComponentsFor) != NotUsed);

        if (NeedToAllocateComponent)
        {
            DecodeBufferManagerStatus_t Status  = AllocateComponent(i, BufferContext);
            if (Status != DecodeBufferManagerNoError)
            {
                SE_ERROR("Failed to allocate all buffer components\n");
                NewDecodeBuffer->DecrementReferenceCount();
                return DecodeBufferManagerFailedToAllocateComponents;
            }

            BufferContext->StructureComponent[i].Usage  = (ComponentData[i].ComponentDescriptor.Usage & NeedComponentsFor);
        }
    }

    // Release the ForDecode buffer immediately, because they are considered as scratch buffers.
    //ReleaseBufferComponents( NewDecodeBuffer, ForDecode );
    *Buffer = NewDecodeBuffer;
    return DecodeBufferManagerNoError;
}


// ---------------------------------------------------------------------
//
//      Functions to get a buffer and allocate all the appropriate components
//

DecodeBufferManagerStatus_t   DecodeBufferManager_Base_c::EnsureReferenceComponentsPresent(
    Buffer_t                Buffer)
{
    unsigned int             i;
    PlayerStatus_t           Status;
    DecodeBufferContextRecord_t     *BufferContext  = GetBufferContext(Buffer);
    bool                 ComponentRelevent;
    bool                 NeedToAllocateComponent;
    OS_LockMutex(&Lock);

    for (i = 0; i < NUMBER_OF_COMPONENTS; i++)
    {
        ComponentRelevent   = ComponentData[i].Enabled &&
                              ((ComponentData[i].ComponentDescriptor.Usage & ForReference) != NotUsed);

        if (ComponentRelevent)
        {
            NeedToAllocateComponent = (BufferContext->StructureComponent[i].Usage == NotUsed);

            if (NeedToAllocateComponent)
            {
                Status  = AllocateComponent(i, BufferContext);

                if (Status != DecodeBufferManagerNoError)
                {
                    SE_ERROR("Failed to allocate all buffer components\n");
                    OS_UnLockMutex(&Lock);
                    return DecodeBufferManagerFailedToAllocateComponents;
                }
            }

            BufferContext->StructureComponent[i].Usage  |= ForReference;
        }
    }

    OS_UnLockMutex(&Lock);
//
    return DecodeBufferManagerNoError;
}


// ---------------------------------------------------------------------
//
//      Function estimate the number of buffers that can be obtained
//

DecodeBufferManagerStatus_t   DecodeBufferManager_Base_c::GetEstimatedBufferCount(unsigned int      *Count,
                                                                                  DecodeBufferUsage_t   For)
{
    unsigned int    i;
    unsigned int    MinimumAvailable;

    MinimumAvailable    = NumberOfDecodeBuffers;

    for (i = 0; i < NUMBER_OF_COMPONENTS; i++)
        if (ComponentData[i].Enabled &&
            ((ComponentData[i].ComponentDescriptor.Usage & For) != NotUsed) &&
            !ComponentData[i].ComponentDescriptor.DataDescriptor->FixedSize &&
            (ComponentData[i].LastAllocationSize != 0))
        {
            MinimumAvailable    = min(MinimumAvailable, (ComponentData[i].AllocationPartitionSize / ComponentData[i].LastAllocationSize));
        }

    *Count  = MinimumAvailable;

    return DecodeBufferManagerNoError;
}


// ---------------------------------------------------------------------
//
//      Function to release a hold on a whole buffer
//
//  This may involve the release of a reference pointer
//

DecodeBufferManagerStatus_t   DecodeBufferManager_Base_c::ReleaseBuffer(Buffer_t    Buffer,
                                                                        bool       ForReference)
{
    DecodeBufferContextRecord_t *BufferContext  = GetBufferContext(Buffer);
    unsigned int             Count;

    //
    // Lock to ensure no-one is releasing individual components
    // when we come to release the whole shebang
    //

    if (ForReference)
    {
        DecrementReferenceUseCount(Buffer);
    }
    else
    {
        OS_LockMutex(&Lock);
        // Can we let the manifestation components go
        Buffer->GetOwnerCount(&Count);

        if (Count == (BufferContext->ReferenceUseCount + 1))
        {
            OS_UnLockMutex(&Lock);
            ReleaseBufferComponents(Buffer, ForManifestation);
            OS_LockMutex(&Lock);
        }

        Buffer->DecrementReferenceCount();
        OS_UnLockMutex(&Lock);
    }

    //
    // Check for any stream switch being over
    //

    if (StreamSwitchOccurring)
    {
        CheckForSwitchStreamTermination();
    }

    return DecodeBufferManagerNoError;
}


// ---------------------------------------------------------------------
//
//      Function to free up components of a buffer, this is a memory
//  management issue, and cannot result in freeing up the whole buffer.
//

DecodeBufferManagerStatus_t   DecodeBufferManager_Base_c::ReleaseBufferComponents(
    Buffer_t         Buffer,
    DecodeBufferUsage_t  ForUse)
{
    unsigned int             i;
    DecodeBufferContextRecord_t *BufferContext  = GetBufferContext(Buffer);

    OS_LockMutex(&Lock);

    for (i = 0; i < NUMBER_OF_COMPONENTS; i++)
        if ((BufferContext->StructureComponent[i].Usage & ForUse) != 0)
        {
            BufferContext->StructureComponent[i].Usage  &= ~ForUse;

            if (BufferContext->StructureComponent[i].Usage == NotUsed)
            {
                // Actually release component
                Buffer->DetachBuffer(BufferContext->StructureComponent[i].Buffer);
                memset(&BufferContext->StructureComponent[i], 0, sizeof(DecodeBufferRequestComponentStructure_t));
            }
        }

    OS_UnLockMutex(&Lock);
    return DecodeBufferManagerNoError;
}


// ---------------------------------------------------------------------
//
//      Functions to handle reference frame usage, allowing us to free
//  up those components that are for reference only at an appropriate
//  point.
//

DecodeBufferManagerStatus_t   DecodeBufferManager_Base_c::IncrementReferenceUseCount(Buffer_t    Buffer)
{
    DecodeBufferContextRecord_t     *BufferContext  = GetBufferContext(Buffer);
    OS_LockMutex(&Lock);
    BufferContext->ReferenceUseCount++;
    Buffer->IncrementReferenceCount();
    OS_UnLockMutex(&Lock);
    return DecodeBufferManagerNoError;
}


// --------------------


DecodeBufferManagerStatus_t   DecodeBufferManager_Base_c::DecrementReferenceUseCount(Buffer_t    Buffer)
{
    DecodeBufferContextRecord_t     *BufferContext  = GetBufferContext(Buffer);
    OS_LockMutex(&Lock);
    BufferContext->ReferenceUseCount--;

    if (BufferContext->ReferenceUseCount == 0)
    {
        OS_UnLockMutex(&Lock);
        ReleaseBufferComponents(Buffer, ForReference);
        OS_LockMutex(&Lock);
    }

    Buffer->DecrementReferenceCount();
    OS_UnLockMutex(&Lock);
    return DecodeBufferManagerNoError;
}


// ---------------------------------------------------------------------
//
//      Accessor functions
//

BufferFormat_t   DecodeBufferManager_Base_c::ComponentDataType(DecodeBufferComponentType_t  Component)
{
    return ComponentData[Component].ComponentDescriptor.DataType;
}

// --------------------

unsigned int    DecodeBufferManager_Base_c::ComponentBorder(DecodeBufferComponentType_t Component,
                                                            unsigned int            Index)
{
    return ComponentData[Component].ComponentDescriptor.ComponentBorders[Index];
}

// --------------------

void    *DecodeBufferManager_Base_c::ComponentAddress(DecodeBufferComponentType_t   Component,
                                                      AddressType_t           AlternateAddressType)
{
    return ComponentData[Component].AllocatorMemory[AlternateAddressType];
}

// --------------------

bool         DecodeBufferManager_Base_c::ComponentPresent(
    Buffer_t            Buffer,
    DecodeBufferComponentType_t Component)
{
    DecodeBufferContextRecord_t     *BufferContext  = GetBufferContext(Buffer);
    return (BufferContext->StructureComponent[Component].Usage != NotUsed);
}

// --------------------

unsigned int     DecodeBufferManager_Base_c::ComponentSize(
    Buffer_t            Buffer,
    DecodeBufferComponentType_t Component)
{
    DecodeBufferContextRecord_t     *BufferContext  = GetBufferContext(Buffer);
    return BufferContext->StructureComponent[Component].Size -  BufferContext->StructureComponent[Component].ComponentOffset[0];
}

// --------------------

void        *DecodeBufferManager_Base_c::ComponentBaseAddress(
    Buffer_t            Buffer,
    DecodeBufferComponentType_t Component)
{
    DecodeBufferContextRecord_t     *BufferContext  = GetBufferContext(Buffer);
    return BufferContext->StructureComponent[Component].BufferBase + BufferContext->StructureComponent[Component].ComponentOffset[0];
}

// --------------------

void        *DecodeBufferManager_Base_c::ComponentBaseAddress(
    Buffer_t            Buffer,
    DecodeBufferComponentType_t Component,
    AddressType_t           AlternateAddressType)
{
    DecodeBufferContextRecord_t *BufferContext  = GetBufferContext(Buffer);
    unsigned char           *BufferBase;
    BufferContext->StructureComponent[Component].Buffer->ObtainDataReference(NULL, NULL, (void **)(&BufferBase), AlternateAddressType);
    return BufferBase + BufferContext->StructureComponent[Component].ComponentOffset[0];
}

// --------------------

unsigned int     DecodeBufferManager_Base_c::ComponentDimension(
    Buffer_t            Buffer,
    DecodeBufferComponentType_t Component,
    unsigned int            DimensionIndex)
{
    DecodeBufferContextRecord_t     *BufferContext  = GetBufferContext(Buffer);
    return BufferContext->StructureComponent[Component].Dimension[DimensionIndex];
}

// --------------------

unsigned int     DecodeBufferManager_Base_c::ComponentStride(
    Buffer_t            Buffer,
    DecodeBufferComponentType_t Component,
    unsigned int            DimensionIndex,
    unsigned int            ComponentIndex)
{
    DecodeBufferContextRecord_t     *BufferContext  = GetBufferContext(Buffer);
    return BufferContext->StructureComponent[Component].Strides[DimensionIndex][ComponentIndex];
}

// --------------------

unsigned int     DecodeBufferManager_Base_c::DecimationFactor(
    Buffer_t            Buffer,
    unsigned int            DecimationIndex)
{
    DecodeBufferContextRecord_t     *BufferContext  = GetBufferContext(Buffer);
    return BufferContext->Request.DecimationFactors[DecimationIndex];
}

// --------------------

unsigned int     DecodeBufferManager_Base_c::LumaSize(Buffer_t          Buffer,
                                                      DecodeBufferComponentType_t Component)
{
    DecodeBufferContextRecord_t     *BufferContext  = GetBufferContext(Buffer);
    return BufferContext->StructureComponent[Component].ComponentOffset[1] - BufferContext->StructureComponent[Component].ComponentOffset[0];
}

// --------------------

unsigned char   *DecodeBufferManager_Base_c::Luma(Buffer_t          Buffer,
                                                  DecodeBufferComponentType_t Component)
{
    DecodeBufferContextRecord_t     *BufferContext  = GetBufferContext(Buffer);
    return BufferContext->StructureComponent[Component].BufferBase + BufferContext->StructureComponent[Component].ComponentOffset[0];
}

// --------------------

unsigned char   *DecodeBufferManager_Base_c::Luma(Buffer_t          Buffer,
                                                  DecodeBufferComponentType_t Component,
                                                  AddressType_t           AlternateAddressType)
{
    DecodeBufferContextRecord_t     *BufferContext  = GetBufferContext(Buffer);
    unsigned char                   *BufferBase;
    BufferContext->StructureComponent[Component].Buffer->ObtainDataReference(NULL, NULL, (void **)(&BufferBase), AlternateAddressType);
    SE_ASSERT(BufferBase != NULL); // cannot be empty
    return BufferBase + BufferContext->StructureComponent[Component].ComponentOffset[0];
}

// --------------------

unsigned char   *DecodeBufferManager_Base_c::LumaInsideBorder(
    Buffer_t            Buffer,
    DecodeBufferComponentType_t Component)
{
    DecodeBufferContextRecord_t     *BufferContext  = GetBufferContext(Buffer);
    unsigned int             Width      = BufferContext->StructureComponent[Component].Dimension[0];
    unsigned int             Side       = ComponentData[Component].ComponentDescriptor.ComponentBorders[0];
    unsigned int             Top        = ComponentData[Component].ComponentDescriptor.ComponentBorders[1];
    unsigned int             BorderOffset   = (Top * Width) + Side;
    return BufferContext->StructureComponent[Component].BufferBase + BufferContext->StructureComponent[Component].ComponentOffset[0] + BorderOffset;
}

// --------------------

unsigned char   *DecodeBufferManager_Base_c::LumaInsideBorder(
    Buffer_t            Buffer,
    DecodeBufferComponentType_t Component,
    AddressType_t           AlternateAddressType)
{
    DecodeBufferContextRecord_t     *BufferContext  = GetBufferContext(Buffer);
    unsigned char                   *BufferBase;
    unsigned int             Width      = BufferContext->StructureComponent[Component].Dimension[0];
    unsigned int             Side       = ComponentData[Component].ComponentDescriptor.ComponentBorders[0];
    unsigned int             Top        = ComponentData[Component].ComponentDescriptor.ComponentBorders[1];
    unsigned int             BorderOffset   = (Top * Width) + Side;
    BufferContext->StructureComponent[Component].Buffer->ObtainDataReference(NULL, NULL, (void **)(&BufferBase), AlternateAddressType);
    SE_ASSERT(BufferBase != NULL); // cannot be empty
    return BufferBase + BufferContext->StructureComponent[Component].ComponentOffset[0] + BorderOffset;
}

// --------------------

unsigned int     DecodeBufferManager_Base_c::ChromaSize(Buffer_t            Buffer,
                                                        DecodeBufferComponentType_t    Component)
{
    DecodeBufferContextRecord_t     *BufferContext  = GetBufferContext(Buffer);
    return BufferContext->StructureComponent[Component].Size - BufferContext->StructureComponent[Component].ComponentOffset[1];
}

// --------------------

unsigned char   *DecodeBufferManager_Base_c::Chroma(Buffer_t            Buffer,
                                                    DecodeBufferComponentType_t Component)
{
    DecodeBufferContextRecord_t     *BufferContext  = GetBufferContext(Buffer);
    return BufferContext->StructureComponent[Component].BufferBase + BufferContext->StructureComponent[Component].ComponentOffset[1];
}

// --------------------

unsigned char   *DecodeBufferManager_Base_c::Chroma(Buffer_t            Buffer,
                                                    DecodeBufferComponentType_t Component,
                                                    AddressType_t           AlternateAddressType)
{
    DecodeBufferContextRecord_t     *BufferContext  = GetBufferContext(Buffer);
    unsigned char                   *BufferBase;
    BufferContext->StructureComponent[Component].Buffer->ObtainDataReference(NULL, NULL, (void **)(&BufferBase), AlternateAddressType);
    SE_ASSERT(BufferBase != NULL); // cannot be empty
    return BufferBase + BufferContext->StructureComponent[Component].ComponentOffset[1];
}

// --------------------

unsigned char   *DecodeBufferManager_Base_c::ChromaInsideBorder(
    Buffer_t            Buffer,
    DecodeBufferComponentType_t Component)
{
    DecodeBufferContextRecord_t     *BufferContext  = GetBufferContext(Buffer);
    unsigned int             Width      = BufferContext->StructureComponent[Component].Dimension[0];
    unsigned int             Side       = ComponentData[Component].ComponentDescriptor.ComponentBorders[0];
    unsigned int             Top        = ComponentData[Component].ComponentDescriptor.ComponentBorders[1];
    unsigned int             BorderOffset   = ((Top * Width) + Side) / 2;
    return BufferContext->StructureComponent[Component].BufferBase + BufferContext->StructureComponent[Component].ComponentOffset[1] + BorderOffset;
}

// --------------------

unsigned char   *DecodeBufferManager_Base_c::ChromaInsideBorder(
    Buffer_t                    Buffer,
    DecodeBufferComponentType_t Component,
    AddressType_t               AlternateAddressType)
{
    DecodeBufferContextRecord_t     *BufferContext  = GetBufferContext(Buffer);
    unsigned char                   *BufferBase;
    unsigned int             Width      = BufferContext->StructureComponent[Component].Dimension[0];
    unsigned int             Side       = ComponentData[Component].ComponentDescriptor.ComponentBorders[0];
    unsigned int             Top        = ComponentData[Component].ComponentDescriptor.ComponentBorders[1];
    unsigned int             BorderOffset   = ((Top * Width) + Side) / 2;
    BufferContext->StructureComponent[Component].Buffer->ObtainDataReference(NULL, NULL, (void **)(&BufferBase), AlternateAddressType);
    SE_ASSERT(BufferBase != NULL); // cannot be empty
    return BufferBase + BufferContext->StructureComponent[Component].ComponentOffset[1] + BorderOffset;
}

// --------------------

unsigned int     DecodeBufferManager_Base_c::RasterSize(Buffer_t            Buffer,
                                                        DecodeBufferComponentType_t    Component)
{
    DecodeBufferContextRecord_t     *BufferContext  = GetBufferContext(Buffer);
    return BufferContext->StructureComponent[Component].Size - BufferContext->StructureComponent[Component].ComponentOffset[0];
}

// --------------------

unsigned char   *DecodeBufferManager_Base_c::Raster(Buffer_t            Buffer,
                                                    DecodeBufferComponentType_t Component)
{
    DecodeBufferContextRecord_t     *BufferContext  = GetBufferContext(Buffer);
    return BufferContext->StructureComponent[Component].BufferBase + BufferContext->StructureComponent[Component].ComponentOffset[0];
}

// --------------------

unsigned char   *DecodeBufferManager_Base_c::Raster(Buffer_t            Buffer,
                                                    DecodeBufferComponentType_t Component,
                                                    AddressType_t           AlternateAddressType)
{
    DecodeBufferContextRecord_t     *BufferContext  = GetBufferContext(Buffer);
    unsigned char                   *BufferBase;
    BufferContext->StructureComponent[Component].Buffer->ObtainDataReference(NULL, NULL, (void **)(&BufferBase), AlternateAddressType);
    SE_ASSERT(BufferBase != NULL); // cannot be empty
    return BufferBase + BufferContext->StructureComponent[Component].ComponentOffset[0];
}

// --------------------



// ---------------------------------------------------------------------
//
//      PRIVATE Function to create a pool for a buffer component
//

DecodeBufferManagerStatus_t   DecodeBufferManager_Base_c::CreateComponentPool(unsigned int  Component)
{
    DecodeBufferComponentData_t *Data   = &ComponentData[Component];
    DecodeBufferComponentData_t *SData  = &StreamSwitchcopyOfComponentData[Component];
    PlayerStatus_t           Status;
    allocator_status_t       AStatus;
    BufferType_t             BufferType;

    //
    // Just check to see if an identical pool already exists in the switch stream set
    //

    if (StreamSwitchOccurring && (SData->BufferPool != NULL) &&
        (strcmp(Data->ComponentDescriptor.DataDescriptor->TypeName, SData->ComponentDescriptor.DataDescriptor->TypeName) == 0) &&
        (strcmp(Data->AllocationPartitionName, SData->AllocationPartitionName) == 0) &&
        (Data->AllocationPartitionSize == SData->AllocationPartitionSize))
    {
        Data->BufferPool            = SData->BufferPool;
        Data->AllocatorMemoryDevice     = SData->AllocatorMemoryDevice;
        Data->AllocatorMemory[CachedAddress]    = SData->AllocatorMemory[CachedAddress];
        Data->AllocatorMemory[PhysicalAddress]  = SData->AllocatorMemory[PhysicalAddress];
        memset(SData, 0, sizeof(DecodeBufferComponentData_t));
        return DecodeBufferManagerNoError;
    }

    //
    // Ensure that the pool type exists
    //
    Status          = BufferManager->FindBufferDataType(Data->ComponentDescriptor.DataDescriptor->TypeName, &BufferType);

    if (Status != BufferNoError)
    {
        Status      = BufferManager->CreateBufferDataType(Data->ComponentDescriptor.DataDescriptor, &BufferType);

        if (Status != BufferNoError)
        {
            SE_ERROR("Failed to create component buffer type '%s'\n",
                     Data->ComponentDescriptor.DataDescriptor->TypeName);
            return DecodeBufferManagerError;
        }
    }

    // Do I need memory - create pool
    // On pool creation, AllowCross64MbBoundary = false as these buffers may be
    // manifested (limitation from VDP requires this alignement)
    switch (Data->ComponentDescriptor.DataDescriptor->AllocationSource)
    {
    case AllocateFromSuppliedBlock :
    {
        if (Data->AllocationPartitionSize > 0)
        {
            unsigned int MemoryAccessType = (Data->ComponentDescriptor.DefaultAddressMode == CachedAddress) ? MEMORY_DEFAULT_ACCESS : MEMORY_VMA_CACHED_ACCESS;
            AStatus = PartitionAllocatorOpen(&Data->AllocatorMemoryDevice, Data->AllocationPartitionName,
                                             Data->AllocationPartitionSize, MemoryAccessType);

            if (AStatus != allocator_ok)
            {
                SE_ERROR("@ Failed to allocate memory (Component %d, Size %d, Partition %s)\n",
                         Component, Data->AllocationPartitionSize, Data->AllocationPartitionName);
                return PlayerInsufficientMemory;
            }

            Data->AllocatorMemory[CachedAddress]    = AllocatorUserAddress(Data->AllocatorMemoryDevice);
            Data->AllocatorMemory[PhysicalAddress]  = AllocatorPhysicalAddress(Data->AllocatorMemoryDevice);
        }
        else
        {
            Data->AllocatorMemory[CachedAddress] = NULL;
            Data->AllocatorMemory[PhysicalAddress] = NULL;
        }
        Status  = BufferManager->CreatePool(&Data->BufferPool, BufferType, NumberOfDecodeBuffers,
                                            Data->AllocationPartitionSize, Data->AllocatorMemory,
                                            NULL, NULL, false);
    }
    break;

    case AllocateIndividualSuppliedBlocks :
        Status  = BufferManager->CreatePool(&Data->BufferPool, BufferType, NumberOfDecodeBuffers,
                                            Data->IndividualBufferSize, NULL, Data->ArrayOfMemoryBlocks,
                                            NULL, false);
        break;

    default:
        Status  = BufferManager->CreatePool(&Data->BufferPool, BufferType, NumberOfDecodeBuffers,
                                            UNSPECIFIED_SIZE, NULL, NULL,
                                            NULL, false);
        break;
    }

    //
    // Check creation
    //

    if (Status != BufferNoError)
    {
        SE_ERROR("Failed to create component pool %d\n", Component);
        return PlayerInsufficientMemory;
    }

//
    return DecodeBufferManagerNoError;
}

// ---------------------------------------------------------------------
//
//      PRIVATE Function to create the actual decode buffer pool
//

DecodeBufferManagerStatus_t   DecodeBufferManager_Base_c::CreateDecodeBufferPool(void)
{
    PlayerStatus_t      Status;
    BufferType_t        DecodeBufferType;

    //
    // If we are called twice, we stick with what we have
    //

    if (DecodeBufferPool != NULL)
    {
        return DecodeBufferManagerNoError;
    }

    //
    // Attempt to register the types for decode buffer
    //
    Player->GetBufferManager(&BufferManager);
    Status      = BufferManager->FindBufferDataType(InitialDecodeBufferContextDescriptor.TypeName, &DecodeBufferType);

    if (Status != BufferNoError)
    {
        Status  = BufferManager->CreateBufferDataType(&InitialDecodeBufferContextDescriptor, &DecodeBufferType);

        if (Status != BufferNoError)
        {
            SE_ERROR("Failed to create the decode buffer type\n");
            SetComponentState(ComponentInError);
            return Status;
        }
    }

    //
    // Create the pool of decode buffers
    //
    Status = BufferManager->CreatePool(&DecodeBufferPool, DecodeBufferType, NumberOfDecodeBuffers);

    if (Status != BufferNoError)
    {
        SE_ERROR("Failed to create the decode buffer pool\n");
        SetComponentState(ComponentInError);
        return Status;
    }

//
    return DecodeBufferManagerNoError;
}

// ---------------------------------------------------------------------
//
//      PRIVATE Function to check the switch stream termination
//

DecodeBufferManagerStatus_t   DecodeBufferManager_Base_c::CheckForSwitchStreamTermination(void)
{
    unsigned int    i;
    unsigned int    BuffersInPool;
    unsigned int    BuffersWithNonZeroReferenceCount;
//
    OS_LockMutex(&Lock);
    StreamSwitchOccurring   = false;

    for (i = 0; i < NUMBER_OF_COMPONENTS; i++)
        if (StreamSwitchcopyOfComponentData[i].BufferPool != NULL)
        {
            StreamSwitchcopyOfComponentData[i].BufferPool->GetPoolUsage(&BuffersInPool, &BuffersWithNonZeroReferenceCount);

            if (BuffersWithNonZeroReferenceCount == 0)
            {
                BufferManager->DestroyPool(StreamSwitchcopyOfComponentData[i].BufferPool);

                AllocatorClose(&StreamSwitchcopyOfComponentData[i].AllocatorMemoryDevice);

                memset(&StreamSwitchcopyOfComponentData[i], 0, sizeof(DecodeBufferComponentData_t));
            }
            else
            {
                StreamSwitchOccurring   = true;
            }
        }

    OS_UnLockMutex(&Lock);
    return DecodeBufferManagerNoError;
}

// ---------------------------------------------------------------------
//
//      PRIVATE Function to check the switch stream termination
//

DecodeBufferManagerStatus_t   DecodeBufferManager_Base_c::AllocateComponent(
    unsigned int             Component,
    DecodeBufferContextRecord_t *BufferContext)
{
    int i;
    PlayerStatus_t               Status;
    DecodeBufferComponentDescriptor_t   *Descriptor;
    DecodeBufferRequestComponentStructure_t *Structure;
    unsigned int                 Size;
    Buffer_t                 ReservedBuffers[MAX_BUFFER_RETRIES + 1];

    if (BufferContext == NULL)
    {
        SE_ERROR("Unable to get a buffer context\n");
        return DecodeBufferManagerError;
    }

    memset(&ReservedBuffers, 0, sizeof(ReservedBuffers));

    // Default values to avoid div by 0 later ...
    for (i = 0; i < MAX_BUFFER_DIMENSIONS; i++)
    {
        if (BufferContext->Request.DecimationFactors[i] == 0)
        {
            BufferContext->Request.DecimationFactors[i] = 1;
        }
    }
    if (BufferContext->Request.MacroblockSize == 0)
    {
        BufferContext->Request.MacroblockSize = 256;
    }
    // Fill out a structure record for this
    Status = FillOutComponentStructure(Component, BufferContext);

    if (Status != DecodeBufferManagerNoError)
    {
        SE_FATAL("Unable to fill out a component structure %d\n", Component);
    }

    //
    // get me a buffer with this in
    //
    Descriptor = &ComponentData[Component].ComponentDescriptor;
    Structure  = &BufferContext->StructureComponent[Component];
    Size       = Descriptor->DataDescriptor->FixedSize ? UNSPECIFIED_SIZE : Structure->Size;

    BufferStatus_t BufferStatus = ComponentData[Component].BufferPool->GetBuffer(&Structure->Buffer, UNSPECIFIED_OWNER, Size);
    if (BufferStatus != BufferNoError)
    {
        BufferStatus_t BufferStatus = ComponentData[Component].BufferPool->GetBuffer(&Structure->Buffer, UNSPECIFIED_OWNER, Size, BufferContext->Request.NonBlockingInCaseOfFailure);
        if (BufferStatus != BufferNoError)
        {
            SE_ERROR("Unable to get a component buffer %d\n", Component);
            return DecodeBufferManagerError;
        }
    }

    Structure->Buffer->ObtainDataReference(NULL, NULL, (void **)(&Structure->BufferBase), Descriptor->DefaultAddressMode);
    //
    // Now attach this to the decode buffer, and give up our hold on it
    //
    BufferContext->DecodeBuffer->AttachBuffer(Structure->Buffer);
    Structure->Buffer->DecrementReferenceCount();
    //
    // Finally record the allocation size of this component
    //
    ComponentData[Component].LastAllocationSize     = Structure->Size;
//
    return DecodeBufferManagerNoError;
}


// ---------------------------------------------------------------------
//
//      PRIVATE Function to check the switch stream termination
//
DecodeBufferManagerStatus_t   DecodeBufferManager_Base_c::FillOutComponentStructure(
    unsigned int             Component,
    DecodeBufferContextRecord_t *BufferContext)
{
    unsigned int i;
    DecodeBufferComponentDescriptor_t   *Descriptor;
    DecodeBufferRequest_t           *Request;
    DecodeBufferRequestComponentStructure_t *Structure;
    //
    // Copy over the dimensions
    //
    Descriptor  = &ComponentData[Component].ComponentDescriptor;
    Request = &BufferContext->Request;
    Structure   = &BufferContext->StructureComponent[Component];
//
    Structure->DimensionCount   = Request->DimensionCount;

    if (Component != DecimatedManifestationComponent)
    {
        for (i = 0; i < Structure->DimensionCount; i++)
        {
            Structure->Dimension[i] = Request->Dimension[i];
        }
    }
    else
    {
        for (i = 0; i < Structure->DimensionCount; i++)
        {
            if (Request->DecimationFactors[i] == 0)
            {
                SE_ERROR("DecimationFactors[%d] == 0 -> should NOT happen!\n", i);
                Structure->Dimension[i] = Request->Dimension[i];
            }
            else
            {
                Structure->Dimension[i] = Request->Dimension[i] / Request->DecimationFactors[i];
            }
        }
    }

    //
    // Fill out a structure record for this component
    //

    switch (Descriptor->DataType)
    {
    case FormatLinear:
        Structure->ComponentCount   = 1;
        Structure->ComponentOffset[0]   = 0;
        Structure->Size         = Request->AdditionalMemoryBlockSize;
        break;

    case FormatPerMacroBlockLinear:
        Structure->Dimension[0]     = BUF_ALIGN_UP(Structure->Dimension[0], 0x3FU);  // Maximal rounding for HEVC 64x64 CTBs
        Structure->Dimension[1]     = BUF_ALIGN_UP(Structure->Dimension[1], 0x3FU);
        Structure->ComponentCount   = 1;
        Structure->ComponentOffset[0]   = 0;
        if (Request->MacroblockSize == 0)
        {
            SE_ERROR("Request->MacroblockSize==0 should NOT happen -> forcing it to 16x16\n");
            Request->MacroblockSize = 256;
        }
        Structure->Size         = Request->PerMacroblockMacroblockStructureSize * ((Structure->Dimension[0] * Structure->Dimension[1]) / Request->MacroblockSize);
        Structure->Size         += Request->PerMacroblockMacroblockStructureFifoSize;
        break;

    case FormatAudio:
        Structure->ComponentCount   = 1;
        Structure->ComponentOffset[0]   = 0;
        Structure->Strides[0][0]    = Structure->Dimension[0] / 8;
        Structure->Strides[1][0]    = Structure->Dimension[1] * Structure->Strides[0][0];
        Structure->Size         = (Structure->Dimension[0] * Structure->Dimension[1] * Structure->Dimension[2]) / 8;
        break;

    case FormatVideo420_PairedMacroBlock:
        //
        // Round up dimensions to paired macroblock size and fall through,
        // NOTE some codecs that do not require paired macroblock widths,
        // do require paired macroblock heights, H264 in particular.
        Structure->Dimension[0]     = BUF_ALIGN_UP(Structure->Dimension[0], 0x1FU);

    case FormatVideo420_MacroBlock:
        Structure->Dimension[0]     = BUF_ALIGN_UP(Structure->Dimension[0], 0x1FU);
        Structure->Dimension[1]     = BUF_ALIGN_UP(Structure->Dimension[1], 0x1FU);
        Structure->ComponentOffset[0]   = 0;
        Structure->ComponentOffset[1]   = Structure->Dimension[0] * Structure->Dimension[1];
        // Force a 1k alignment, to support the 7109 hardware mpeg2 decoder
        Structure->ComponentOffset[1]   = BUF_ALIGN_UP(Structure->ComponentOffset[1], 0x3FFU);
        Structure->Strides[0][0]    = Structure->Dimension[0];
        Structure->Strides[0][1]    = Structure->Dimension[0];
        Structure->Size         = Structure->ComponentOffset[1] + ((Structure->Dimension[0] * Structure->Dimension[1]) / 2);
        Structure->ComponentCount   = 2;
        break;

    case FormatVideo420_Raster2B:
        if (Descriptor->ComponentBorders[0] == 16)
        {
            Structure->Dimension[0]            = BUF_ALIGN_UP(Structure->Dimension[0], 0xFU);
            Structure->Dimension[1]            = BUF_ALIGN_UP(Structure->Dimension[1], 0xFU);
        }
        else if (Descriptor->ComponentBorders[0] == 64)
        {
            Structure->Dimension[0]            = BUF_ALIGN_UP(Structure->Dimension[0], 0x3FU);
            Structure->Dimension[1]            = BUF_ALIGN_UP(Structure->Dimension[1], 0x3FU);
        }
        else
        {
            // alignment on width is required because of decode hw constraints whereas
            // alignment on height is required because of display constraint
            Structure->Dimension[0]            = BUF_ALIGN_UP(Structure->Dimension[0], 0x1FU);
            Structure->Dimension[1]            = BUF_ALIGN_UP(Structure->Dimension[1], 0x1FU);
        }

        Structure->ComponentOffset[0]      = 0;
        Structure->ComponentOffset[1]      = Structure->Dimension[0] * Structure->Dimension[1];
        Structure->Strides[0][0]           = Structure->Dimension[0];
        Structure->Strides[0][1]           = Structure->Dimension[0];
        Structure->Size                    = Structure->ComponentOffset[1] +
                                             ((Structure->Dimension[0] * Structure->Dimension[1]) / 2);
        Structure->ComponentCount          = 2;
        break;

    case FormatVideo422_Raster:
        //
        // Round up dimension 0 to 32, these buffers are used for dvp capture,
        // the capture hardware needs strides on a 16 byte boundary and
        // the display hardware needs strides on a 64 byte (32 pixel) boundary.
        //
        Structure->Dimension[0]            = BUF_ALIGN_UP(Structure->Dimension[0], 0x1FU);
        Structure->ComponentCount          = 1;
        Structure->ComponentOffset[0]      = 0;
        Structure->Strides[0][0]           = 2 * Structure->Dimension[0];
        Structure->Size                    = (2 * Structure->Dimension[0] * Structure->Dimension[1]);
        break;

    case FormatVideo420_Planar:
        Structure->Dimension[0]            = BUF_ALIGN_UP(Structure->Dimension[0] + Descriptor->ComponentBorders[0] * 2, 0xFU);
        Structure->Dimension[1]            = BUF_ALIGN_UP(Structure->Dimension[1] + Descriptor->ComponentBorders[1] * 2, 0xFU);
        Structure->ComponentCount          = 2;
        Structure->ComponentOffset[0]      = 0;
        Structure->ComponentOffset[1]      = Structure->Dimension[0] * Structure->Dimension[1];
        Structure->Strides[0][0]           = Structure->Dimension[0];
        Structure->Strides[0][1]           = Structure->Dimension[0] / 2;
        Structure->Size                    = (Structure->ComponentOffset[1] * 3) / 2;
        break;

    case FormatVideo420_PlanarAligned:
        Structure->Dimension[0]            = BUF_ALIGN_UP(Structure->Dimension[0] + Descriptor->ComponentBorders[0] * 2, 0x3FU);
        Structure->Dimension[1]            = BUF_ALIGN_UP(Structure->Dimension[1] + Descriptor->ComponentBorders[1] * 2, 0xFU);
        Structure->ComponentCount          = 2;
        Structure->ComponentOffset[0]      = 0;
        Structure->ComponentOffset[1]      = Structure->Dimension[0] * Structure->Dimension[1];
        Structure->Strides[0][0]           = Structure->Dimension[0];
        Structure->Strides[0][1]           = Structure->Dimension[0] / 2;
        Structure->Size                    = (Structure->ComponentOffset[1] * 3) / 2;
        break;

    case FormatVideo422_Planar:
        Structure->Dimension[0]            = BUF_ALIGN_UP(Structure->Dimension[0] + Descriptor->ComponentBorders[0] * 2, 0x3FU);
        Structure->Dimension[1]            = BUF_ALIGN_UP(Structure->Dimension[1] + Descriptor->ComponentBorders[1] * 2, 0x3FU);
        Structure->ComponentCount          = 2;
        Structure->ComponentOffset[0]      = 0;
        Structure->ComponentOffset[1]      = Structure->Dimension[0] * Structure->Dimension[1];
        Structure->Strides[0][0]           = Structure->Dimension[0];
        Structure->Strides[0][1]           = Structure->Dimension[0];
        Structure->Size                    = (Structure->ComponentOffset[1] * 2);
        break;

    case FormatVideo8888_ARGB:
        Structure->Dimension[0]            = BUF_ALIGN_UP(Structure->Dimension[0], 0x1FU);
        Structure->Dimension[1]            = BUF_ALIGN_UP(Structure->Dimension[1], 0x1FU);
        Structure->ComponentCount          = 1;
        Structure->ComponentOffset[0]      = 0;
        Structure->Strides[0][0]           = Structure->Dimension[0] * 4;
        Structure->Size                    = (Structure->Dimension[0] * Structure->Dimension[1] * 4);
        break;

    case FormatVideo888_RGB:
        Structure->Dimension[0]            = BUF_ALIGN_UP(Structure->Dimension[0], 0x1FU);
        Structure->Dimension[1]            = BUF_ALIGN_UP(Structure->Dimension[1], 0x1FU);
        Structure->ComponentCount          = 1;
        Structure->ComponentOffset[0]      = 0;
        Structure->Strides[0][0]           = Structure->Dimension[0] * 3;
        Structure->Size                    = (Structure->Dimension[0] * Structure->Dimension[1] * 3);
        break;

    case FormatVideo565_RGB:
        Structure->Dimension[0]            = BUF_ALIGN_UP(Structure->Dimension[0], 0x1FU);
        Structure->Dimension[1]            = BUF_ALIGN_UP(Structure->Dimension[1], 0x1FU);
        Structure->ComponentCount          = 1;
        Structure->ComponentOffset[0]      = 0;
        Structure->Strides[0][0]           = Structure->Dimension[0] * 2;
        Structure->Size                    = (Structure->Dimension[0] * Structure->Dimension[1] * 2);
        break;

    case FormatVideo422_YUYV:
        Structure->Dimension[0]            = BUF_ALIGN_UP(Structure->Dimension[0], 0x1FU);
        Structure->Dimension[1]            = BUF_ALIGN_UP(Structure->Dimension[1], 0x1FU);
        Structure->ComponentCount          = 1;
        Structure->ComponentOffset[0]      = 0;
        Structure->Strides[0][0]           = Structure->Dimension[0] * 2;
        Structure->Size                    = (Structure->Dimension[0] * Structure->Dimension[1] * 2);
        break;

    case FormatVideo422_Raster2B:
        if (Descriptor->ComponentBorders[0] == 16)
        {
            Structure->Dimension[0]        = BUF_ALIGN_UP(Structure->Dimension[0], 0xFU);
            Structure->Dimension[1]        = BUF_ALIGN_UP(Structure->Dimension[1], 0xFU) ;
        }
        else if (Descriptor->ComponentBorders[0] == 64)
        {
            Structure->Dimension[0]        = BUF_ALIGN_UP(Structure->Dimension[0], 0x3FU);
            Structure->Dimension[1]        = BUF_ALIGN_UP(Structure->Dimension[1], 0x3FU);
        }
        else
        {
            Structure->Dimension[0]        = BUF_ALIGN_UP(Structure->Dimension[0], 0x1FU);
            Structure->Dimension[1]        = BUF_ALIGN_UP(Structure->Dimension[1], 0x1FU);
        }
        Structure->ComponentCount          = 2;
        Structure->ComponentOffset[0]      = 0;
        Structure->ComponentOffset[1]      = Structure->Dimension[0] * Structure->Dimension[1];
        Structure->Strides[0][0]           = Structure->Dimension[0];
        Structure->Strides[0][1]           = Structure->Dimension[0];
        Structure->Size                    = (Structure->Dimension[0] * Structure->Dimension[1] * 2);
        break;

    case FormatVideo444_Raster:
        Structure->Dimension[0]            = BUF_ALIGN_UP(Structure->Dimension[0], 0x1FU);
        Structure->Dimension[1]            = BUF_ALIGN_UP(Structure->Dimension[1], 0x1FU);
        Structure->ComponentCount          = 1;
        Structure->ComponentOffset[0]      = 0;
        Structure->Strides[0][0]           = Structure->Dimension[0] * 3;
        Structure->Size                    = (Structure->Dimension[0] * Structure->Dimension[1] * 3);
        break;

    default:
        SE_ERROR("Unknown buffer format %d\n", Descriptor->DataType);
        return DecodeBufferManagerError;
    }

    return DecodeBufferManagerNoError;
}


// ---------------------------------------------------------------------
//
//      PRIVATE Function to get the buffer context
//

DecodeBufferContextRecord_t   *DecodeBufferManager_Base_c::GetBufferContext(Buffer_t     Buffer)
{
    DecodeBufferContextRecord_t     *Result;
    Buffer->ObtainDataReference(NULL, NULL, (void **)(&Result));
    return Result;
}
