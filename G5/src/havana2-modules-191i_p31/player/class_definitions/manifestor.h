/************************************************************************
Copyright (C) 2006 STMicroelectronics. All Rights Reserved.

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

Source file name : manifestor.h
Author :           Nick

Definition of the manifestor class module for player 2.


Date        Modification                                    Name
----        ------------                                    --------
01-Nov-06   Created                                         Nick

************************************************************************/

#ifndef H_MANIFESTOR
#define H_MANIFESTOR

#include "player.h"

// ---------------------------------------------------------------------
//
// Local type definitions
//

enum
{
    ManifestorNoError                           = PlayerNoError,
    ManifestorError                             = PlayerError,

    ManifestorUnplayable                       = BASE_MANIFESTOR,
    ManifestorWouldBlock,
    ManifestorNullQueued
};

typedef PlayerStatus_t  ManifestorStatus_t;

//

enum
{
    ManifestorFnGetDecodeBufferPool             = BASE_MANIFESTOR,
    ManifestorFnRegisterOutputBufferRing,
    ManifestorFnSetSurface,
    ManifestorFnGetManifestationDescriptor,
    ManifestorFnGetNextQueuedManifestationTime,
    ManifestorFnReleaseQueuedDecodeBuffers,
    ManifestorFnQueueDecodeBuffer,
    ManifestorFnQueueNullManifestation,
    ManifestorFnQueueEventSignal,

    ManifestorVideoFnSetOutputWindow,		// These are extensions in the video manifestor, that we need to support inline calling for
    ManifestorVideoFnSetInputWindow,

    ManifestorFnSetModuleParameters
};

// ---------------------------------------------------------------------
//
// Class definition
//

class Manifestor_c : public BaseComponentClass_c
{
public:

    virtual ManifestorStatus_t   GetDecodeBufferPool(           BufferPool_t             *Pool ) = 0;

    virtual ManifestorStatus_t   GetPostProcessControlBufferPool(BufferPool_t            *Pool ) = 0;

    virtual ManifestorStatus_t   RegisterOutputBufferRing(      Ring_t                    Ring ) = 0;

    virtual ManifestorStatus_t   GetSurfaceParameters(          void                    **SurfaceParameters ) = 0;

    virtual ManifestorStatus_t   GetNextQueuedManifestationTime(unsigned long long       *Time) = 0;

    virtual ManifestorStatus_t   ReleaseQueuedDecodeBuffers(    void ) = 0;

    virtual ManifestorStatus_t   InitialFrame(                  Buffer_t                  Buffer ) = 0;

    virtual ManifestorStatus_t   QueueDecodeBuffer(             Buffer_t                  Buffer ) = 0;

    virtual ManifestorStatus_t   QueueNullManifestation(        void ) = 0;

    virtual ManifestorStatus_t   QueueEventSignal(              PlayerEventRecord_t      *Event ) = 0;

    virtual ManifestorStatus_t   GetNativeTimeOfCurrentlyManifestedFrame(unsigned long long *Time) = 0;

    virtual ManifestorStatus_t   GetDecodeBuffer(               BufferStructure_t        *RequestedStructure,
								Buffer_t                 *Buffer ) = 0;

    virtual ManifestorStatus_t   GetDecodeBufferCount(          unsigned int             *Count ) = 0;

    virtual ManifestorStatus_t   SynchronizeOutput(             void ) = 0;

    virtual ManifestorStatus_t   GetFrameCount(                 unsigned long long       *FrameCount) = 0;

};

// ---------------------------------------------------------------------
//
// Docuentation
//

/*! \class Manifestor_c
    \brief The manifestor class is responsible for taking individual decoded frames and manifesting them.

    It creates the pool of decode buffers, and handles manifestation of those buffers.

    The partial list of meta data types used by this class :-

    Attached to input buffers :-

     - ParsedFrameParameters Describes the frame to be manifested.
     - Parsed[Video|Audio|Data]Parameters Optional input providing 
       further control parameters to the manifestation process.
     - [Video|Audio|Data]OutputTiming Mandatory input of one of 
       these providing control parameters to the manifestation process.

    Added to output buffers :-

     - [Video|Audio|Data]OutputTiming Modified to incorporate the actual timing information.
*/

/*! \fn ManifestorStatus_t Manifestor_c::GetDecodeBufferPool(BufferPool_t *Pool)
    \brief Gain access to (and potentially allocate) the codec's buffer pool.

    On the first call to this function after class construction, or a reset, the function 
    will ensure that the decode buffer pool exists (this may involve the actual creation of
    the pool). On the first and all subsequent calls to this function it will return the buffer 
    pool.

    \param Pool A pointer to a variable to hold the instance 
		of a buffer pool containing the decode buffers.
    \return Manifestor status code, ManifestorNoError indicates success.
*/

/*! \fn ManifestorStatus_t Manifestor_c::RegisterOutputBufferRing(Ring_t Ring)
    \brief Specify the ring to receive buffers after manifestation

    Calling this function indicates to the manifestor where it is to place decode buffers when 
    their manifestation is complete. Though fairly innocuous looking, this is a critical function, 
    after this is called the manifestor is expected to be in a running state, that is it should be 
    ready to accept buffers for display, and other function calls.

    \param Ring A pointer to a ring instance to take used buffers
    \return Manifestor status code, ManifestorNoError indicates success.
*/

/*! \fn ManifestorStatus_t Manifestor_c::GetSurfaceParameters(void      **SurfaceParameters)
    \brief Gain access to a manifestor maintained structure describing parameters of the output surface.

    The manifestor is responsible for maintaining a descriptor of the output surface parameters that
    may affect the behaviour of other components of the system such as the output timer. This function 
    allows those other components to gain access to a pointer to that structure. The memory address associated 
    with that pointer should remain valid until a call to reset(). The actual structure type should be 
    one of VideoOutputSurfaceDescriptor_t, AudioOutputSurfaceDescriptor_t or DataOutputSurfaceDescriptor_t 
    depending on the nature of the manifestor.

    \param SurfaceParameters A pointer to a variable to hold a pointer to one of [Video|Audio|Data]SurfaceDescriptor_t.
    \return Manifestor status code, ManifestorNoError indicates success.
*/

/*! \fn ManifestorStatus_t Manifestor_c::GetNextQueuedManifestationTime(unsigned long long *Time)
    \brief Estimate the time that the next frame enqueued will be displayed.

    I wanted to call this "Get The Earliest System Time At Which The 
    Next Frame To Be Queued Will Be Manifested" but it was too long. This function must
    report the earliest time at which the manifestor can \b guarantee to present a frame
    if it were queued instantaneously. If the manifestors output is not 
    currently operating the manifestor must estimate the worse case startup time and report that.

    This is a function used by the output timer in order to establish a playback time/system time mapping, and 
    to allow the output timer to make informed decisions about whether or not a frame 
    should be dropped because it has been decoded too late for manifestation.

    \param Time A pointer to a system time value, to be filled with the
		estimated time.
    \return Manifestor status code, ManifestorNoError indicates success.
*/

/*! \fn ManifestorStatus_t Manifestor_c::ReleaseQueuedDecodeBuffers(void)
    \brief Discard all possible queued decode buffers.

    Passes onto the output ring any decode buffers that are currently 
    queued, but not in the process of being manifested, or imminently manifested. 
    The buffers so
    released, can be discarded (as in fast channel change), or 
    re-submitted (as in a re-timing operation due to speed change), or
    a combination of the two. It is acceptable for a buffer to be held back if 
    it is due to be manifested in a very short time (say 10ms), in order that 
    glitches be avoided when a re-timing is being performed.

    \return Manifestor status code, ManifestorNoError indicates success.
*/

/*! \fn ManifestorStatus_t Manifestor_c::InitialFrame(Buffer_t Buffer)
    \brief Indicate initial frame.

       This function will be called by the player as soon as a buffer becomes 
available iff (if an only if for those who have never seen iff before) the 
policy PolicyManifestFirstFrameEarly is set for the specific stream,playback or 
player.

       The buffer passed in will NOT have timing information associated with it, 
it will contain the decoded data along with any other meta data items normally 
associated with a decode buffer (exempting the timing data).

       For Audio, I would expect the policy not to be set, and the manifestor to 
return either ManifestorNoError, or PlayerNotSupported.

       For Video, I would expect the frame to be diplayed (if the fn is called) 
until a frame is received via the normal mechanism. The top field first, and 
interlaced flag, should be obtained from ParsedVideoParameters (rather than the 
usual timing structure), and if interlaced then only the temporally first field 
(dependant on the value of top field first) should be displayed (using 
de-interlacing if necessary).

       The buffer passed to this function should be used until the first buffer 
is received by the normal route, at this point it should be forgotten, IT MUST 
NOT BE PLACED ON THE OUTPUT RING. Note it is very very likely that this buffer 
will be the first buffer to be submitted via the normal route.


    \param Buffer A pointer to an instance of a decode buffer, as output
	   from a codec.
    \return Manifestor status code, ManifestorNoError indicates success.
*/

/*! \fn ManifestorStatus_t Manifestor_c::QueueDecodeBuffer(Buffer_t Buffer)
    \brief Queue a buffer for manifestion.

    Provide a buffer to the manifestor to allow manifestation of it's contents.

    \param Buffer A pointer to an instance of a decode buffer, as output
	   from a codec.
    \return Manifestor status code, ManifestorNoError indicates success.
*/

/*! \fn ManifestorStatus_t Manifestor_c::QueueNullManifestation(void)
    \brief Insert blank, or silence, in the manifestion queue.

    Insertion is such that the screen will go blank, or the speaker 
    will go silent, after the last queued buffer has been manifested.
    The null manifestation will terminate on the commencement of
    manifestation of a new buffer.

    \return Manifestor status code, ManifestorNoError indicates success.
*/

/*! \fn ManifestorStatus_t Manifestor_c::QueueEventSignal(PlayerEventRecord_t *Event)
    \brief Request that a manifestion event be emitted.

    A call to this function informs the manifestor that the specified event record 
    is to be copied, and supplied to the player fn SignalEvent() at the appropriate time.

    There are three possible times when the event should be signalled. As the first frame
    is manifested, if no previous buffer was queued.
    After the last 
    queued buffer completes manifestation, and as the next one commences manifestation. 
    After the last queued buffer completes manifestation, and as the manifestation is terminated 
    whether due to a null manifestation or because the manifestor is to be halted.

    To be clear, this function may be called multiple times between buffers, and all the 
    events so queued must be signalled at the appropriate time.

    \param Event A pointer to a player 2 event record, to be signalled. 

    \return Manifestor status code, ManifestorNoError indicates success.
*/

/*! \fn ManifestorStatus_t Manifestor_c::GetNativeTimeOfCurrentlyManifestedFrame(unsigned long long *Time)
    \brief Request the native time of the currently visible (audible) frame.

    \param Time A pointer to a native time value, to be filled with the time.
    \return Manifestor status code, ManifestorNoError indicates success.
*/

/*! \fn ManifestorStatus_t Manifestor_c::GetDecodeBuffer( BufferStructure_t  *RequestedStructure, Buffer_t  *Buffer )
    \brief Obtain a decode buffer capable of satisfying the specified structural request.

    This function is responsible for taking a buffer structure request, and obtaining a buffer. 
    The request specifies the type and dimensions of the buffer, and the function fleshes 
    that structure description out, and attaches a copy to the actual buffer as meta data.

    \param RequestedStructure, a description of the type of content and the buffer dimensions.
    \param Buffer, the returned buffer.
    \return Manifestor status code, ManifestorNoError indicates success.
*/

/*! \fn ManifestorStatus_t Manifestor_c::GetDecodeBufferCount( unsigned int  *Count )
    \brief Obtain a count of the decode buffers that can be allocated matching the last specified structure request.

    This function derives a count of the number of decode buffers that can be allocated at any one time, 
    given the particular structure description. This count is very useful in the functioning of adaptive 
    modes of reverse play.

    \param Count, the returned count.
    \return Manifestor status code, ManifestorNoError indicates success.
*/

/*! \fn ManifestorStatus_t Manifestor_c::SynchronizeOutput( void )
    \brief Synchronize output so that any framing of data aligns with the current time

    This function largely applies to mechanisms with discrete frame outputs that are widely separated 
    in time (ie video). It forces the frame boundary to allign (as closely as possible) withe the current 
    time.

    \return Manifestor status code, ManifestorNoError indicates success.
*/
#endif

