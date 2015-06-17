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

#ifndef ENCODER_GENERIC_H
#define ENCODER_GENERIC_H

#include "encoder.h"

#define BUFFER_ENCODER_CONTROL_STRUCTURE    "EncoderControlStructure"
#define BUFFER_ENCODER_CONTROL_STRUCTURE_TYPE   {BUFFER_ENCODER_CONTROL_STRUCTURE, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(EncoderControlStructure_t)}

#define BUFFER_INPUT                             "InputBuffer"
#define BUFFER_INPUT_TYPE                        {BUFFER_INPUT, BufferDataTypeBase, NoAllocation, 1, 0, false, false, 0}

// check BufferDataDescriptor_t in buffer_manager.h
// HasFixedSize = true
// AllocateOnPoolCreation = false
// if close caption payload is included, we need a variable size and set FixedSize=0
#define METADATA_ENCODE_FRAME_PARAMETERS         "EncodeFrameParameters"
#define METADATA_ENCODE_FRAME_PARAMETERS_TYPE    {METADATA_ENCODE_FRAME_PARAMETERS, MetaDataTypeBase, AllocateFromOSMemory, 4, 0, true, false, sizeof(stm_se_uncompressed_frame_metadata_t)}

#define METADATA_ENCODE_COORDINATOR_FRAME_PARAMETERS         "EncodeCoordinatorFrameParameters"
#define METADATA_ENCODE_COORDINATOR_FRAME_PARAMETERS_TYPE    {METADATA_ENCODE_COORDINATOR_FRAME_PARAMETERS, MetaDataTypeBase, AllocateFromOSMemory, 4, 0, true, false, sizeof(__stm_se_encode_coordinator_metadata_t)}

class Encoder_Generic_c : public Encoder_c
{
public:
    EncoderStatus_t  InitializationStatus;

    Encoder_Generic_c(void);
    ~Encoder_Generic_c(void);

    //
    // Managing the Encoder
    //

    EncoderStatus_t   CreateEncode(Encode_t    *Encode);
    EncoderStatus_t   TerminateEncode(Encode_t  Encode);

    EncoderStatus_t   CreateEncodeStream(EncodeStream_t                  *EncodeStream,
                                         Encode_t                         Encode,
                                         stm_se_encode_stream_media_t     Media,
                                         stm_se_encode_stream_encoding_t  Encoding);
    EncoderStatus_t   TerminateEncodeStream(Encode_t                      Encode,
                                            EncodeStream_t                EncodeStream);

    EncoderStatus_t   CallInSequence(EncodeStream_t            Stream,
                                     EncodeSequenceType_t      Type,
                                     EncodeSequenceValue_t     Value,
                                     EncodeComponentFunction_t Fn,
                                     ...);

    //
    // Mechanisms for registering global items
    //

    EncoderStatus_t   RegisterBufferManager(BufferManager_t BufferManager);

    const EncoderBufferTypes *GetBufferTypes(void);

    EncoderStatus_t   RegisterScaler(Scaler_t    Scaler);

    EncoderStatus_t   RegisterBlitter(Blitter_t  Blitter);

    //
    // Support functions for the child classes
    //

    EncoderStatus_t   GetBufferManager(BufferManager_t      *BufferManager);

    EncoderStatus_t   GetClassList(EncodeStream_t            Stream,
                                   Encode_t                 *Encode,
                                   Preproc_t                *Preproc           = NULL,
                                   Coder_t                  *Coder             = NULL,
                                   Transporter_t            *Transporter       = NULL,
                                   EncodeCoordinator_t      *EncodeCoordinator = NULL);

    EncoderStatus_t   GetInputBuffer(Buffer_t  *Buffer, bool NonBlocking = false);

    EncoderStatus_t   InputData(EncodeStream_t            Stream,
                                const void               *DataVirtual,
                                unsigned long             DataPhysical,
                                unsigned int              DataSize,
                                const stm_se_uncompressed_frame_metadata_t *Metadata);

    //
    // Low power functions
    //
    EncoderStatus_t   LowPowerEnter(__stm_se_low_power_mode_t low_power_mode);
    EncoderStatus_t   LowPowerExit(void);

    __stm_se_low_power_mode_t   GetLowPowerMode(void);

private:
    // "AudioEncodeNo" should be incremented whenever an AudioStream in added and decremented whenever deleted.
    // (from the deleted stream to ensure that next added stream will take the place of this deleted stream).
    // Should be used to set corresponding stream's AudioEncodeNo.
    // Its range being modulo ENCODER_STREAM_AUDIO_MAX_CPU.
    uint32_t AudioEncodeNo;

    OS_Mutex_t                   Lock;

    // Linked list of existing encode instances
    Encode_t                     ListOfEncodes;

    BufferManager_t              BufferManager;

    BufferPool_t                 InputBufferPool;

    // The Buffer types are stored structurally for
    // simplicity in registering across multiple components
    EncoderBufferTypes           BufferTypes;

    // Save the current low power mode in case of standby
    // so that it can be read by the various encode objects
    __stm_se_low_power_mode_t    LowPowerMode;

    DISALLOW_COPY_AND_ASSIGN(Encoder_Generic_c);
};

// ---------------------------------------------------------------------
//
// Documentation
//

/*! \class Encoder_Generic_c
\brief Implementation of the Singleton Encoder Class.

This class is the glue that holds all the components. Its methods form
the primary means for the encoder wrapper to manipulate an encoder instance.

*/

#endif /* ENCODER_GENERIC_H */
