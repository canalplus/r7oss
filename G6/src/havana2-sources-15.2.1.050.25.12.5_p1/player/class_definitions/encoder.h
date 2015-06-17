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

#ifndef H_ENCODER
#define H_ENCODER

#include "player.h"

#include "encoder_types.h"

#include "encode.h"
#include "preproc.h"
#include "coder.h"
#include "encode_stream.h"
#include "transporter.h"

class Encoder_c
{
public:
    EncoderStatus_t              InitializationStatus;

    Encoder_c(void) : InitializationStatus(EncoderNoError) {};
    virtual ~Encoder_c(void) {};

    //
    // Managing the Encoder
    //

    virtual EncoderStatus_t   CreateEncode(Encode_t        *Encode) = 0;

    virtual EncoderStatus_t   TerminateEncode(Encode_t      Encode) = 0;

    virtual EncoderStatus_t   CreateEncodeStream(EncodeStream_t                   *EncodeStream,
                                                 Encode_t                          Encode,
                                                 stm_se_encode_stream_media_t      Media,
                                                 stm_se_encode_stream_encoding_t   Encoding) = 0;

    virtual EncoderStatus_t   TerminateEncodeStream(Encode_t           Encode,
                                                    EncodeStream_t     EncodeStream) = 0;

    virtual EncoderStatus_t   CallInSequence(EncodeStream_t            Stream,
                                             EncodeSequenceType_t      Type,
                                             EncodeSequenceValue_t     Value,
                                             EncodeComponentFunction_t Fn,
                                             ...) = 0;

    //
    // Mechanisms for registering global items
    //

    virtual EncoderStatus_t   RegisterBufferManager(BufferManager_t    BufferManager) = 0;

    virtual EncoderStatus_t   RegisterScaler(Scaler_t                  Scaler) = 0;

    virtual EncoderStatus_t   RegisterBlitter(Blitter_t                Blitter) = 0;

    //
    // Support functions for the child classes
    //

    virtual EncoderStatus_t   GetBufferManager(BufferManager_t      *BufferManager) = 0;

    virtual const EncoderBufferTypes *GetBufferTypes(void) = 0;

    virtual EncoderStatus_t   GetClassList(EncodeStream_t            Stream,
                                           Encode_t                 *Encode,
                                           Preproc_t                *Preproc,
                                           Coder_t                  *Coder,
                                           Transporter_t            *Transporter,
                                           EncodeCoordinator_t      *EncodeCoordinator) = 0;

    virtual EncoderStatus_t   GetInputBuffer(Buffer_t  *Buffer, bool NonBlocking = false) = 0;

    //
    // Low power functions
    //

    virtual EncoderStatus_t   LowPowerEnter(__stm_se_low_power_mode_t low_power_mode) = 0;

    virtual EncoderStatus_t   LowPowerExit(void) = 0;

    virtual __stm_se_low_power_mode_t GetLowPowerMode(void) = 0;
};

// ---------------------------------------------------------------------
//
// Documentation
//

/*! \class Encoder_c
\brief Glue all the other components together.

This class is the glue that holds all the components. Its methods form
the primary means for the encoder wrapper to manipulate an encoder instance.

*/

/*! \var EncoderStatus_t Encoder_c::InitializationStatus;
\brief Status flag indicating how the initialization of the class went.

In order to fit well into the Linux kernel environment the streaming engine
is compiled with exceptions and RTTI disabled. In C++ constructors cannot
return an error code since all errors should be reported using
exceptions. With exceptions disabled we need to use this alternative
means to communicate that the constructed object instance is not valid.

*/

//
// Mechanisms for registering global items
//

/*! \fn EncoderStatus_t   Encoder_c::RegisterBufferManager(     BufferManager_t           BufferManager )
\brief Register a buffer manager.

Register a player-global buffer manager.

\param BufferManager A pointer to a BufferManager_c instance.

\return Encoder status code, EncoderNoError indicates success.
*/

/*! \fn EncoderStatus_t   Encoder_c::RegisterScaler(     Scaler_t                  Scaler )
\brief Register a Scaler Device with the encoder

Provide an instance of a Scaler device which can be used by the pre-processors to perform video frame operations

\param Scaler A pointer to a Scaler_t instance.

\return Encoder status code, EncoderNoError indicates success.
*/

/*! \fn EncoderStatus_t   Encoder_c::RegisterBlitter(     Blitter_t                 Blitter )
\brief Register a Blitter Device with the encoder

Provide an instance of a Blitter device which can be used by the pre-processors to perform video frame operations

\param Scaler A pointer to a Blitter_t instance.

\return Encoder status code, EncoderNoError indicates success.
*/

//
// Support functions for the child classes
//

/*! \fn EncoderStatus_t   Encoder_c::GetBufferManager(     BufferManager_t          *BufferManager )
\brief Obtain a pointer to the buffer manager.

<b>WARNING</b>: This is an internal function and should only be called by player components.

\param BufferManager Pointer to a variable to hold a pointer to the BufferManager_c instance.

\return Encoder status code, EncoderNoError indicates success.
*/

/*! \fn EncoderStatus_t   Encoder_c::GetClassList(  EncodeStream_t            Stream,
                            Preproc_t                *Preproc,
                            Coder_t                  *Coder,
                            Transporter_t            *Transporter,
                            EncodeCoordinator_t      *EncodeCoordinator )
\brief Obtain pointers to the encoder components used by the specified stream.

Note: Only those pointers that are non-NULL will be filled in.

<b>WARNING</b>: This is an internal function and should only be called by encoder components.

\param Stream      Stream identifier
\param Preproc     Pointer to a variable to hold a pointer to the Preproc_c instance, or NULL.
\param Coder       Pointer to a variable to hold a pointer to the Coder_c instance, or NULL.
\param Transporter Pointer to a variable to hold a pointer to the Transporter_c instance, or NULL.
\param EncodeCoordinator  Pointer to a variable to hold a pointer to the EncodeCoordinator_c instance, or NULL.

\return Encoder status code, EncoderNoError indicates success.
*/

/*! \fn EncodeStreamStatistics_t   Encoder_c::GetStatistics(     EncodeStream_t  Stream )
\brief Return a copy of all the statistics collected by the stream.

Returns all of the statistics provided by the stream specified at the time of calling.

\param Stream A pointer to an EncodeStream_t.

\return EncoderStreamStatistics_t
*/

/*! \fn EncoderStatus_t   Encoder_c::ResetStatistics(     EncodeStream_t  Stream )
\brief Reset all statistics collected in a stream.

Zeros down all internal data members used to store stream based statistics

\param Stream A pointer to an EncodeStream_t.

\return Encoder status code, EncoderNoError indicates success.
*/

#endif /* H_ENCODER */
