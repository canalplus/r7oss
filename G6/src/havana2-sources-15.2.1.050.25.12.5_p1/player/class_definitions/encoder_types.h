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

#ifndef ENCODER_TYPES_H
#define ENCODER_TYPES_H

//
// We are an extension of player types
//

#include "player_types.h"

// Forward declaration of a BufferType
// We can't include buffer.h here.
typedef unsigned int    BufferType_t;

// /////////////////////////////////////////////////////////////////////////
//
// The wrapper pointer types, here before the structures to
// allow for those self referential structures (such as lists),
// and incestuous references.
//

typedef PlayerStatus_t  EncoderStatus_t;

enum
{
    EncoderNoError          = PlayerNoError,
    EncoderError            = PlayerError,

    EncoderUnknownError     = BASE_ENCODER,

    EncoderNotOpen,
    EncoderNoDevice,
    EncoderNoMemory,
    EncoderImplementationError,
    EncoderNotSupported,
    EncoderTooManyStreams,
    EncoderUnknownStream,
    EncoderMatchNotFound,
    EncoderNoEventRecords,
    EncoderUnsupportedControl, // returned by set/get controls when it is not supported yet
    EncoderTimedOut,
    EncoderBusy,
    EncoderControlNotMatch, // returned by set/get controls when it does not apply to the current component
};

// TODO: These types must be removed when they are defined by their implementations
typedef void       *Scaler_t;
typedef void       *Blitter_t;

typedef PlayerStreamType_t      EncodeStreamType_t;

#ifdef __cplusplus
typedef class Encoder_c         *Encoder_t;
typedef class Encode_c          *Encode_t;
typedef class Preproc_c         *Preproc_t;
typedef class Coder_c           *Coder_t;
typedef class Transporter_c     *Transporter_t;
typedef class EncodeStream_c    *EncodeStream_t;
typedef class EncodeCoordinatorInterface_c   *EncodeCoordinator_t;
#endif // __cplusplus

typedef PlayerComponent_t   EncoderComponent_t;

typedef enum VideoEncodeMemoryProfile_e
{
    EncodeProfileHD      = STM_SE_CTRL_VALUE_ENCODE_HD_PROFILE,
    EncodeProfile720p    = STM_SE_CTRL_VALUE_ENCODE_720p_PROFILE,
    EncodeProfileSD      = STM_SE_CTRL_VALUE_ENCODE_SD_PROFILE,
    EncodeProfileCIF     = STM_SE_CTRL_VALUE_ENCODE_CIF_PROFILE,
} VideoEncodeMemoryProfile_t;

//
// Specifiers for in sequence calling
//

// /////////////////////////////////////////////////////////////////////////
//
// The Buffer sequence number meta data type, allowing the
// attachment of a sequence number to all coded buffers.
//

typedef struct EncoderSequenceNumber_s
{
    bool                    MarkerFrame;
    unsigned long long      Value;

    // Statistical values for each buffer

    unsigned long long      TimeEntryInProcess0;
    unsigned long long      DeltaEntryInProcess0;
    unsigned long long      TimeEntryInProcess1;
    unsigned long long      DeltaEntryInProcess1;

    unsigned long long      TimePassToPreprocessor;
    unsigned long long      TimePassToCoder;
    unsigned long long      TimePassToOutput;
} EncoderSequenceNumber_t;

#define ENCODE_METADATA_SEQUENCE_NUMBER     "EncodeSequenceNumber"
#define ENCODE_METADATA_SEQUENCE_NUMBER_TYPE    {ENCODE_METADATA_SEQUENCE_NUMBER, MetaDataTypeBase, AllocateFromOSMemory, 4, 0, true, false, sizeof(EncoderSequenceNumber_t)}

enum
{
    // Encode Component Functions
    EncoderFnFirstFunction = BASE_ENCODER,

};

typedef unsigned int EncodeComponentFunction_t;

typedef PlayerSequenceType_t EncodeSequenceType_t;

typedef unsigned long long EncodeSequenceValue_t;

// /////////////////////////////////////////////////////////////////////////
//
// The event record
//

/// An EncoderEventRecord_t is an exact correlation to a PlayerEventRecord_t so we can re-use its type
typedef PlayerEventRecord_t EncoderEventRecord_t;

// /////////////////////////////////////////////////////////////////////////
//
// The encoder control structure buffer type and associated structures
//

#define ENCODER_MAX_INLINE_PARAMETER_BLOCK_SIZE     64

typedef enum
{
    EncodeActionInSequenceCall  = 0
} EncodeControlAction_t;

typedef struct EncodeInSequenceParams_s
{
    EncodeComponentFunction_t  Fn;
    unsigned int               UnsignedInt;
    void                      *Pointer;
    unsigned char              Block[ENCODER_MAX_INLINE_PARAMETER_BLOCK_SIZE];
    EncoderEventRecord_t       Event;

} EncodeInSequenceParams_t;

typedef struct EncodeControlStructure_s
{
    EncodeControlAction_t      Action;
    EncodeSequenceType_t       SequenceType;
    EncodeSequenceValue_t      SequenceValue;

    EncodeInSequenceParams_t   InSequence;

} EncodeControlStructure_t;

#define BUFFER_ENCODE_CONTROL_STRUCTURE     "EncodeControlStructure"
#define BUFFER_ENCODE_CONTROL_STRUCTURE_TYPE    {BUFFER_ENCODE_CONTROL_STRUCTURE, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(EncodeControlStructure_t)}

// /////////////////////////////////////////////////////////////////////////
//
// The encoder must share the buffer types across all of its worker classes
//

typedef struct EncoderBufferTypes_s
{
    //
    // Input Buffer Types
    // These buffers have no 'storage' for the raw data.
    // They are used to point to external data sources.
    //

    BufferType_t                 BufferInputBufferType;
    BufferType_t                 BufferEncoderControlStructureType;

    // EncodeStream Buffer Types
    // These buffers must be allocated by the relevant worker class
    // and will contain output data processed by that class.

    BufferType_t                 PreprocFrameBufferType;
    BufferType_t                 PreprocFrameAllocType;
    BufferType_t                 CodedFrameBufferType;

    //
    // MetaData Buffer types will be allocated as required.
    //

    BufferType_t                 InputMetaDataBufferType;
    BufferType_t                 EncodeCoordinatorMetaDataBufferType;
    BufferType_t                 InternalMetaDataBufferType;
    BufferType_t                 MetaDataSequenceNumberType;

} EncoderBufferTypes;

typedef struct EncodeStreamStatistics_s
{
    //preproc counters
    unsigned int        BufferCountToPreproc;
    unsigned int        FrameCountToPreproc;
    unsigned int        DiscontinuityBufferCountToPreproc;
    unsigned int        BufferCountFromPreproc;
    unsigned int        DiscontinuityBufferCountFromPreproc;
    //coder counters
    unsigned int        BufferCountToCoder;
    unsigned int        FrameCountFromCoder;
    unsigned int        EosBufferCountFromCoder;
    unsigned int        VideoEncodeFrameSkippedCountFromCoder;
    //transporter counters
    unsigned int        BufferCountToTransporter;
    unsigned int        BufferCountFromTransporter;
    unsigned int        NullSizeBufferCountFromTransporter;
    //TsMux specific
    unsigned int        ReleaseBufferCountFromTsMuxTransporter;
    //errors counters
    unsigned int        TsMuxQueueError;
    unsigned int        TsMuxTransporterBufferAddressError;
    unsigned int        TsMuxTransporterUnexpectedReleasedBufferError;
    unsigned int        TsMuxTransporterRingExtractError;
} EncodeStreamStatistics_t;

#endif /* ENCODER_TYPES_H */
