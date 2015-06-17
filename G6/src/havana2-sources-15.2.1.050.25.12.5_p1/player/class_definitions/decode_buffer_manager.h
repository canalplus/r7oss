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

#ifndef H_DECODE_BUFFER_MANAGER
#define H_DECODE_BUFFER_MANAGER

#include "player.h"
#include "allocinline.h"

enum
{
    DecodeBufferManagerNoError              = PlayerNoError,
    DecodeBufferManagerError                = PlayerError,

    DecodeBufferManagerFailedToAllocateComponents   = BASE_DECODE_BUFFER_MANAGER,
};

typedef PlayerStatus_t  DecodeBufferManagerStatus_t;

#if defined CONFIG_32BIT || defined CONFIG_ARM
#define MAX_DECODE_BUFFERS      64
#else
#define MAX_DECODE_BUFFERS      32
#endif

#define MAX_BUFFER_DIMENSIONS   5
#define MAX_BUFFER_COMPONENTS   5

typedef enum
{
    PrimaryManifestationComponent   = 0,
    DecimatedManifestationComponent,
    VideoDecodeCopy,
    VideoMacroblockStructure,
    VideoPostProcessControl,
    AdditionalMemoryBlock,
    UndefinedComponent
} DecodeBufferComponentType_t;

#define NUMBER_OF_COMPONENTS    UndefinedComponent

enum DecodeBufferComponentElementMask_e
{
    PrimaryManifestationElement     = 1,
    DecimatedManifestationElement   = 2,
    VideoDecodeCopyElement          = 4,
    VideoMacroblockStructureElement = 8,
    VideoPostProcessControlElement  = 16,
    AdditionalMemoryBlockElement    = 32
};

typedef unsigned int DecodeBufferComponentElementMask_t;

enum DecodeBufferUsage_e
{
    NotUsed          = 0,
    ForDecode        = 1,
    ForManifestation = 2,
    ForReference     = 4
};
typedef unsigned int DecodeBufferUsage_t;

typedef enum
{
    FormatUnknown                       = 0,
    FormatMarkerFrame,
    FormatLinear,
    FormatPerMacroBlockLinear,
    FormatAudio,
    FormatVideo420_MacroBlock,
    FormatVideo420_PairedMacroBlock,
//    FormatVideo420_Raster_Y_CbCr,
//    FormatVideo420_Raster_Y_Cb_Cr,
    FormatVideo422_Raster,
    FormatVideo420_Planar,
    FormatVideo420_PlanarAligned,
    FormatVideo422_Planar,
    FormatVideo8888_ARGB,
    FormatVideo888_RGB,
    FormatVideo565_RGB,
    FormatVideo422_YUYV,
    FormatVideo420_Raster2B,
    FormatVideo422_Raster2B,
    FormatVideo444_Raster
} BufferFormat_t;

// ---------------------------------------------------------------------
// Structure to define the components that make up a decode buffer
//

typedef struct DecodeBufferComponentDescriptor_s
{
    DecodeBufferComponentType_t  Type;
    DecodeBufferUsage_t          Usage;

    BufferFormat_t           DataType;
    AddressType_t            DefaultAddressMode;
    BufferDataDescriptor_t  *DataDescriptor;

    unsigned int             ComponentBorders[MAX_BUFFER_COMPONENTS];
} DecodeBufferComponentDescriptor_t;

// ---------------------------------------------------------------------
// Structure to define a buffer request, filled in, holds most of the
// useful info about a decode buffer.
//

typedef struct DecodeBufferRequest_s
{
    //
    // Fields filled in for the request
    //

    bool           MarkerFrame;
    bool           NonBlockingInCaseOfFailure;

    unsigned int   DimensionCount;
    unsigned int   Dimension[MAX_BUFFER_DIMENSIONS];
    unsigned int   DecimationFactors[MAX_BUFFER_DIMENSIONS];


    bool           ReferenceFrame;
    unsigned int   MacroblockSize;
    unsigned int   PerMacroblockMacroblockStructureSize;
    unsigned int   PerMacroblockMacroblockStructureFifoSize;
    unsigned int   AdditionalMemoryBlockSize;
} DecodeBufferRequest_t;


// ---------------------------------------------------------------------
// Structure to define the elements of a reserved memory list -
//  held by the player, and managed by instances of the
//  decode buffer manager to allow usage spanning playbacks for
//  memory left on display.
//

#define MAXIMUM_NUMBER_OF_RESERVED_MANIFESTATION_MEMORY_BLOCKS  1

typedef struct DecodeBufferReservedBlock_s
{
    unsigned char       *BlockBase;
    unsigned int         BlockSize;
    allocator_device_t   AllocatorMemoryDevice;
    BufferPool_t         BufferPool;
} DecodeBufferReservedBlock_t;

//
class DecodeBufferManager_c : public BaseComponentClass_c
{
public:
    //
    // Function to obtain the buffer pool, in order to attach meta data to the whole pool
    //

    virtual DecodeBufferManagerStatus_t   GetDecodeBufferPool(BufferPool_t *Pool) = 0;

    //
    // Function to support entry into stream switch mode
    //

    virtual DecodeBufferManagerStatus_t   EnterStreamSwitch(void) = 0;

    //
    // Functions to specify the component list, and to enable or
    // disable the inclusion of specific components in a decode buffer.
    //

    virtual DecodeBufferManagerStatus_t   FillOutDefaultList(BufferFormat_t                      PrimaryFormat,
                                                             DecodeBufferComponentElementMask_t  Elements,
                                                             DecodeBufferComponentDescriptor_t   List[]) = 0;

    virtual DecodeBufferManagerStatus_t   InitializeComponentList(DecodeBufferComponentDescriptor_t *List) = 0;
    virtual DecodeBufferManagerStatus_t   InitializeSimpleComponentList(BufferFormat_t  Type) = 0;

    virtual DecodeBufferManagerStatus_t   ModifyComponentFormat(DecodeBufferComponentType_t  Component,
                                                                BufferFormat_t               NewFormat) = 0;

    virtual DecodeBufferManagerStatus_t   ComponentEnable(DecodeBufferComponentType_t Component,
                                                          bool                        Enabled) = 0;

    virtual DecodeBufferManagerStatus_t   ArchiveReservedMemory(DecodeBufferReservedBlock_t     *Blocks) = 0;
    virtual DecodeBufferManagerStatus_t   InitializeReservedMemory(DecodeBufferReservedBlock_t  *Blocks) = 0;

    virtual DecodeBufferManagerStatus_t   ReserveMemory(void *Base, unsigned int Size) = 0;

    virtual DecodeBufferManagerStatus_t   AbortGetBuffer(void) = 0;
    virtual void DisplayComponents() = 0;
    virtual void ResetDecoderMap(void) = 0;
    virtual DecodeBufferManagerStatus_t   CreateDecoderMap(void) = 0;

    //
    // Functions to get/release a decode buffer, or to free up some of the components when they are no longer needed
    //

    virtual DecodeBufferManagerStatus_t   GetDecodeBuffer(DecodeBufferRequest_t  *Request,
                                                          Buffer_t               *Buffer) = 0;
    virtual DecodeBufferManagerStatus_t   EnsureReferenceComponentsPresent(Buffer_t Buffer) = 0;
    virtual DecodeBufferManagerStatus_t   GetEstimatedBufferCount(unsigned int        *Count,
                                                                  DecodeBufferUsage_t  For = ForManifestation) = 0;
    virtual DecodeBufferManagerStatus_t   ReleaseBuffer(Buffer_t  Buffer,
                                                        bool      ForReference) = 0;
    virtual DecodeBufferManagerStatus_t   ReleaseBufferComponents(Buffer_t            Buffer,
                                                                  DecodeBufferUsage_t ForUse) = 0;

    virtual DecodeBufferManagerStatus_t   IncrementReferenceUseCount(Buffer_t Buffer) = 0;
    virtual DecodeBufferManagerStatus_t   DecrementReferenceUseCount(Buffer_t Buffer) = 0;

    //
    // Accessors - in order to access pointers etc
    //

    virtual BufferFormat_t   ComponentDataType(DecodeBufferComponentType_t  Component) = 0;
    virtual unsigned int     ComponentBorder(DecodeBufferComponentType_t    Component,
                                             unsigned int            Index) = 0;
    virtual void        *ComponentAddress(DecodeBufferComponentType_t   Component,
                                          AddressType_t           AlternateAddressType) = 0;

    virtual bool         ComponentPresent(Buffer_t                    Buffer,
                                          DecodeBufferComponentType_t Component) = 0;

    virtual unsigned int     ComponentSize(Buffer_t                    Buffer,
                                           DecodeBufferComponentType_t Component) = 0;

    virtual void        *ComponentBaseAddress(Buffer_t                    Buffer,
                                              DecodeBufferComponentType_t Component) = 0;
    virtual void        *ComponentBaseAddress(Buffer_t                      Buffer,
                                              DecodeBufferComponentType_t   Component,
                                              AddressType_t                 AlternateAddressType) = 0;
    virtual unsigned int     ComponentDimension(Buffer_t                    Buffer,
                                                DecodeBufferComponentType_t Component,
                                                unsigned int                DimensionIndex) = 0;
    virtual unsigned int     ComponentStride(Buffer_t                    Buffer,
                                             DecodeBufferComponentType_t Component,
                                             unsigned int                DimensionIndex,
                                             unsigned int                ComponentIndex) = 0;

    virtual unsigned int     DecimationFactor(Buffer_t          Buffer,
                                              unsigned int      DecimationIndex) = 0;


    virtual unsigned int     LumaSize(Buffer_t                    Buffer,
                                      DecodeBufferComponentType_t Component) = 0;
    virtual unsigned char   *Luma(Buffer_t                    Buffer,
                                  DecodeBufferComponentType_t Component) = 0;
    virtual unsigned char   *Luma(Buffer_t                    Buffer,
                                  DecodeBufferComponentType_t Component,
                                  AddressType_t               AlternateAddressType) = 0;
    virtual unsigned char   *LumaInsideBorder(Buffer_t                    Buffer,
                                              DecodeBufferComponentType_t Component) = 0;
    virtual unsigned char   *LumaInsideBorder(Buffer_t                    Buffer,
                                              DecodeBufferComponentType_t Component,
                                              AddressType_t               AlternateAddressType) = 0;

    virtual unsigned int     ChromaSize(Buffer_t                    Buffer,
                                        DecodeBufferComponentType_t Component) = 0;
    virtual unsigned char   *Chroma(Buffer_t                    Buffer,
                                    DecodeBufferComponentType_t Component) = 0;
    virtual unsigned char   *Chroma(Buffer_t                    Buffer,
                                    DecodeBufferComponentType_t Component,
                                    AddressType_t               AlternateAddressType) = 0;
    virtual unsigned char   *ChromaInsideBorder(Buffer_t                    Buffer,
                                                DecodeBufferComponentType_t Component) = 0;
    virtual unsigned char   *ChromaInsideBorder(Buffer_t                    Buffer,
                                                DecodeBufferComponentType_t Component,
                                                AddressType_t               AlternateAddressType) = 0;

    virtual unsigned int     RasterSize(Buffer_t                    Buffer,
                                        DecodeBufferComponentType_t Component) = 0;
    virtual unsigned char   *Raster(Buffer_t                    Buffer,
                                    DecodeBufferComponentType_t Component) = 0;
    virtual unsigned char   *Raster(Buffer_t                    Buffer,
                                    DecodeBufferComponentType_t Component,
                                    AddressType_t               AlternateAddressType) = 0;
};

#endif
