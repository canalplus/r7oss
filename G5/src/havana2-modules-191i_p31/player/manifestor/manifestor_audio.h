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

Source file name : manifestor_audio.h
Author :           Daniel

Definition of the manifestor audio class implementation for player 2.


Date        Modification                                    Name
----        ------------                                    --------
14-May-07   Created (from manifestor_video.h)               Daniel

************************************************************************/

#ifndef H_MANIFESTOR_AUDIO
#define H_MANIFESTOR_AUDIO

#include "osdev_user.h"
#include "player.h"
#include "manifestor_base.h"

typedef enum
{
    AudioBufferStateAvailable,
    AudioBufferStateQueued,
    AudioBufferStateNotQueued
} AudioBufferState_t;

////////////////////////////////////////////////////////////////////////////
///
/// Additional information about a buffer.
///
struct AudioStreamBuffer_s
{
    unsigned int BufferIndex;
    unsigned int QueueCount;
    unsigned int TimeOfGoingOnDisplay;
    bool EventPending;

    AudioBufferState_t BufferState;

    class Buffer_c* Buffer; 

    unsigned char* Data; ///< Pointer to the data buffer (a cached address)
    ParsedFrameParameters_t *FrameParameters; ///< Frame parameters for the buffer.
    bool UpdateAudioParameters; ///< True, if this buffer has a significant change in audio parameters.
    ParsedAudioParameters_t *AudioParameters; ///< Audio parameters for the buffer.
    AudioOutputTiming_t *AudioOutputTiming; ///< Audio output timing of the buffer.
    
    bool QueueAsCodedData; ///< True if the encoded buffer should be enqueued for display.

    unsigned int NextIndex; ///< Index of the next buffer to be displayed or -1 if no such buffer exists.
};


typedef struct ManifestorAudioParameterBlock_s
{
    ManifestorParameterBlockType_t      ParameterType;

    union
    {
	void                            *Mixer;
	bool				EmergencyMute;
    };
} ManifestorAudioParameterBlock_t;


////////////////////////////////////////////////////////////////////////////
///
/// Framework for implementing audio manifestors.
///
class Manifestor_Audio_c : public Manifestor_Base_c
{
private:

    unsigned int RelayfsIndex; //stores id from relayfs to differentiate the manifestor instance

    /* Generic stream information*/

protected:

    bool                                Visible;
    bool                                DisplayUpdatePending;

    /* Display Information */
    struct AudioOutputSurfaceDescriptor_s       SurfaceDescriptor;

    /* Buffer information */
    struct AudioStreamBuffer_s          StreamBuffer[MAXIMUM_NUMBER_OF_DECODE_BUFFERS];
    unsigned int                        BufferQueueHead;
    unsigned int                        BufferQueueTail;
    OS_Mutex_t                          BufferQueueLock;
    OS_Event_t                          BufferQueueUpdated;
    unsigned int                        QueuedBufferCount;
    unsigned int                        NotQueuedBufferCount;

    bool                                DestroySyncrhonizationPrimatives;

    /// The audio parameters of the most recently \b enqueued buffer.
    ParsedAudioParameters_t             LastSeenAudioParameters;

    /* Data shared with buffer release process */
    bool                                ForcedUnblock;

    virtual ManifestorStatus_t  QueueBuffer            (unsigned int                    BufferIndex) = 0;
    virtual ManifestorStatus_t  ReleaseBuffer          (unsigned int                    BufferIndex) = 0;
    ManifestorStatus_t DequeueBuffer(unsigned int *BufferIndexPtr, bool NonBlock = true);
    void               PushBackBuffer(unsigned int BufferInex);

public:

    /* Constructor / Destructor */
    Manifestor_Audio_c                                  (void);
    ~Manifestor_Audio_c                                 (void);

    /* Overrides for component base class functions */
    ManifestorStatus_t   Halt                           (void);
    ManifestorStatus_t   Reset                          (void);

    /* Manifestor class functions */
    ManifestorStatus_t   GetDecodeBufferPool            (class BufferPool_c**   Pool);
    ManifestorStatus_t   GetSurfaceParameters           (void**                 SurfaceParameters);
    ManifestorStatus_t   GetNextQueuedManifestationTime (unsigned long long*    Time);
    ManifestorStatus_t   ReleaseQueuedDecodeBuffers     (void);
    ManifestorStatus_t   InitialFrame                   (class Buffer_c*        Buffer);
    ManifestorStatus_t   QueueDecodeBuffer              (class Buffer_c*        Buffer);
    ManifestorStatus_t   GetNativeTimeOfCurrentlyManifestedFrame       (unsigned long long*     Pts);
    ManifestorStatus_t   GetFrameCount                  (unsigned long long*    FrameCount);

    unsigned int         GetBufferId                    (void);

    /* these virtual functions are implemented by the device specific part of the audio manifestor */
    virtual ManifestorStatus_t  OpenOutputSurface      (void) = 0;
    virtual ManifestorStatus_t  CloseOutputSurface     (void) = 0;

    // Methods to be supplied by this derived classes
    ManifestorStatus_t   FillOutBufferStructure( BufferStructure_t	 *RequestedStructure );

};
#endif
