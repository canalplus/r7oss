/************************************************************************
Copyright (C) 2007 STMicroelectronics. All Rights Reserved.

This file is part of the Player2 Library.

Player2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Player2 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Player2 Library may alternatively be licensed under a proprietary
license from ST.

Source file name : manifestor_base.h
Author :           Julian

Definition of the manifestor base class implementation for player 2.


Date        Modification                                    Name
----        ------------                                    --------
11-Jan-07   Created                                         Julian

************************************************************************/

#ifndef H_MANIFESTOR_BASE
#define H_MANIFESTOR_BASE

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "player.h"
#include "allocinline.h"

// /////////////////////////////////////////////////////////////////////
//
//      defined values

#ifdef CONFIG_32BIT
#define MAXIMUM_NUMBER_OF_DECODE_BUFFERS        64
#else
#define MAXIMUM_NUMBER_OF_DECODE_BUFFERS        32
#endif

#define MAXIMUM_WAITING_EVENTS                  32
#define INVALID_BUFFER_ID                       0xffffffff
#define ANY_BUFFER_ID                           0xfffffffe

//      Debug printing macros


#ifndef ENABLE_MANIFESTOR_DEBUG
#define ENABLE_MANIFESTOR_DEBUG                 0
#endif

#define MANIFESTOR_TAG                          "ManifestorBase_c::"
#define MANIFESTOR_FUNCTION                     __FUNCTION__

/* Output debug information (which may be on the critical path) but is usually turned off */
#define MANIFESTOR_DEBUG(fmt, args...) ((void)(ENABLE_MANIFESTOR_DEBUG && \
                                          (report(severity_note, "%s%s: " fmt, MANIFESTOR_TAG, MANIFESTOR_FUNCTION, ##args), 0)))

/* Output trace information off the critical path */
#define MANIFESTOR_TRACE(fmt, args...) (report(severity_note, "%s%s: " fmt, MANIFESTOR_TAG, MANIFESTOR_FUNCTION, ##args))

/* Output errors, should never be output in 'normal' operation */
#define MANIFESTOR_ERROR(fmt, args...) (report(severity_error, "%s%s: " fmt, MANIFESTOR_TAG, MANIFESTOR_FUNCTION, ##args))

#define MANIFESTOR_FATAL(fmt, args...) (report(severity_fatal, "%s%s: " fmt, MANIFESTOR_TAG, MANIFESTOR_FUNCTION, ##args))

#define MANIFESTOR_ASSERT(x) do if(!(x)) report(severity_error, "%s: Assertion '%s' failed at %s:%d\n", \
                                               MANIFESTOR_FUNCTION, #x, __FILE__, __LINE__); while(0)

// ---------------------------------------------------------------------
//
// Local type definitions
//

    //
    // The internal configuration for the manifestor base
    //

typedef struct ManifestorConfiguration_s
{
    const char                   *ManifestorName;
    PlayerStreamType_t            StreamType;

    BufferDataDescriptor_t       *DecodeBufferDescriptor;
    unsigned int                  PostProcessControlBufferCount;

    unsigned int                  OutputRateSmoothingFramesBetweenReCalculate;
} ManifestorConfiguration_t;

    //
    // The set parameters definitions for defining decode buffer memory
    //

typedef enum
{
    ManifestorBufferConfiguration       = BASE_MANIFESTOR,
    ManifestorAudioMixerConfiguration,
    ManifestorAudioSetEmergencyMuteState,
} ManifestorParameterBlockType_t;

//

typedef struct ManifestorBufferConfiguration_s
{
    BufferFormat_t              DecodedBufferFormat;
    unsigned int                MaxBufferCount;
    unsigned int                TotalBufferMemory;
    char                        PartitionName[ALLOCATOR_MAX_PARTITION_NAME_SIZE];
} ManifestorBufferConfiguration_t;

//

typedef struct ManifestorParameterBlock_s
{
    ManifestorParameterBlockType_t      ParameterType;

    union
    {
        ManifestorBufferConfiguration_t  BufferConfiguration;
    };
} ManifestorParameterBlock_t;

//

struct EventRecord_s
{
    unsigned int                Id;
    struct PlayerEventRecord_s  Event;
};

// ---------------------------------------------------------------------
//
// Class definition
//

/// Framework for implementing manifestors.
class Manifestor_Base_c : public Manifestor_c
{
protected:

    // Data

    ManifestorConfiguration_t             Configuration;

    BufferManager_t                       BufferManager;
    ManifestorBufferConfiguration_t       BufferConfiguration;

    // Buffers
    BufferDataDescriptor_t               *DecodeBufferDescriptor;
    BufferType_t                          DecodeBufferType;
    allocator_device_t                    DecodeMemoryDevice;
    void                                 *DecodeBufferMemory[3];
    BufferPool_t                          DecodeBufferPool;
    unsigned int                          LastDecodeBufferSize;

    BufferPool_t                          PostProcessControlBufferPool;

    //  Output ring for used buffers
    Ring_t                      OutputRing;

    /* Output rate smoothing data */

    unsigned int                         OutputRateSmoothingFramesSinceLastCalculation;
    unsigned int                         OutputRateSmoothingIndex;
    Rational_t                           OutputRateSmoothingLastRate;
    Rational_t                           OutputRateSmoothingSubPPMPart;
    unsigned int                         OutputRateSmoothingBaseValue;
    unsigned int                         OutputRateSmoothingLastValue;
    bool				 OutputRateMovingTo;

    /* Event control */
    bool                        EventPending;
    struct EventRecord_s        EventList[MAXIMUM_WAITING_EVENTS];
    OS_Mutex_t                  EventLock;
    bool                        EventLockValid;
    unsigned int                NextEvent;
    unsigned int                LastEvent;

    ManifestorStatus_t   ServiceEventQueue              (unsigned int           Id);
    ManifestorStatus_t   FlushEventQueue                (void);

    virtual unsigned int GetBufferId                    (void) = 0;
    virtual ManifestorStatus_t FlushDisplayQueue        (void) = 0;

public:

    // Constructor/Destructor methods
    Manifestor_Base_c                                   (void);
    ~Manifestor_Base_c                                  (void);

    // Overrides for component base class functions
    ManifestorStatus_t   Halt                           (void);
    ManifestorStatus_t   Reset                          (void);
    ManifestorStatus_t   SetModuleParameters            (unsigned int           ParameterBlockSize,
                                                         void*                  ParameterBlock);

    // Manifestor class functions
    ManifestorStatus_t   GetDecodeBufferPool            (BufferPool_t          *Pool);
    ManifestorStatus_t   GetPostProcessControlBufferPool(BufferPool_t          *Pool);
    ManifestorStatus_t   RegisterOutputBufferRing       (Ring_t                 Ring);
    ManifestorStatus_t   GetSurfaceParameters           (void**                 SurfaceParameters);
    ManifestorStatus_t   GetNextQueuedManifestTime      (unsigned long long*    Time);
    ManifestorStatus_t   ReleaseQueuedDecodeBuffers     (void);
    ManifestorStatus_t   QueueNullManifestation         (void);
    ManifestorStatus_t   QueueEventSignal               (PlayerEventRecord_t*   Event);

    ManifestorStatus_t   GetDecodeBuffer(               BufferStructure_t        *RequestedStructure,
                                                        Buffer_t                 *Buffer );

    ManifestorStatus_t   GetDecodeBufferCount(          unsigned int             *Count );

    ManifestorStatus_t   SynchronizeOutput(             void );

    // Support functions for derived classes

    ManifestorStatus_t   DerivePPMValueFromOutputRateAdjustment(
                                                        Rational_t               OutputRateAdjustment,
                                                        int                     *PPMValue );

    ManifestorStatus_t   ValidatePhysicalDecodeBufferAddress( 
							unsigned int		 Address );

    // Methods to be supplied by the derived classes

    virtual ManifestorStatus_t   FillOutBufferStructure(BufferStructure_t        *RequestedStructure )
        { return ManifestorError; }
    virtual bool         BufferAvailable(               unsigned char          *Start,
                                                        unsigned int            Size)
        { return true; }

};
#endif
