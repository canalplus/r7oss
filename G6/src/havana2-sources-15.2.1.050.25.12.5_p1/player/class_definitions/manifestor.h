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

#ifndef H_MANIFESTOR
#define H_MANIFESTOR

#include "player.h"

#define TUNEABLE_NAME_AUDIO_MANIFESTOR_ENABLE_CRC   "audio_manifestor_enable_crc"

enum
{
    ManifestorNoError        = PlayerNoError,
    ManifestorError          = PlayerError,

    ManifestorUnplayable     = BASE_MANIFESTOR,
    ManifestorWouldBlock,
    ManifestorNullQueued,
    ManifestorRenderComplete,
};

typedef PlayerStatus_t  ManifestorStatus_t;

//

enum
{
    ManifestorFnConnect      = BASE_MANIFESTOR,
    ManifestorFnSetSurface,
    ManifestorFnGetManifestationDescriptor,
    ManifestorFnGetNextQueuedManifestationTime,
    ManifestorFnReleaseQueuedDecodeBuffers,
    ManifestorFnQueueDecodeBuffer,
    ManifestorFnQueueNullManifestation,
    ManifestorFnQueueEventSignal,

    ManifestorFnSetModuleParameters
};

class Manifestor_c : public BaseComponentClass_c
{
public:
    static unsigned int volatile EnableAudioCRC;

    virtual ManifestorStatus_t   Connect(Port_c *Port) = 0;

    virtual ManifestorStatus_t   GetSurfaceParameters(OutputSurfaceDescriptor_t **SurfaceParameters, unsigned int *NumSurfaces) = 0;

    virtual ManifestorStatus_t   GetNumberOfTimings(unsigned int *NumTimes) = 0;

    virtual ManifestorStatus_t   GetNextQueuedManifestationTime(unsigned long long *Time, unsigned int *NumTimes) = 0;

    virtual ManifestorStatus_t   ReleaseQueuedDecodeBuffers(void) = 0;

    virtual ManifestorStatus_t   QueueDecodeBuffer(Buffer_t                      Buffer,
                                                   ManifestationOutputTiming_t **TimingArray,
                                                   unsigned int                 *NumTimes) = 0;

    virtual ManifestorStatus_t   QueueNullManifestation(void) = 0;

    virtual ManifestorStatus_t   QueueEventSignal(PlayerEventRecord_t *Event) = 0;

    virtual ManifestorStatus_t   GetNativeTimeOfCurrentlyManifestedFrame(unsigned long long *Time) = 0;

    virtual ManifestorStatus_t   SynchronizeOutput(void) = 0;

    virtual ManifestorStatus_t   GetFrameCount(unsigned long long       *FrameCount) = 0;

    virtual ManifestorStatus_t   GetCapabilities(unsigned int             *Capabilities) = 0;


    virtual void                 ResetOnStreamSwitch(void) = 0;

    static void RegisterTuneable(void)
    {
        // Register the SW CRC option global
        OS_RegisterTuneable(TUNEABLE_NAME_AUDIO_MANIFESTOR_ENABLE_CRC, (unsigned int *)&EnableAudioCRC);
    };
    static void UnregisterTuneable(void)
    {
        // Register the SW CRC option global
        OS_UnregisterTuneable(TUNEABLE_NAME_AUDIO_MANIFESTOR_ENABLE_CRC);
    };
};

// ---------------------------------------------------------------------
//
// Documentation
//

/*! \class Manifestor_c
    \brief The manifestor class is responsible for taking individual decoded frames and manifesting them.

    It handles manifestation of decode buffers.

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

/*! \fn ManifestorStatus_t Manifestor_c::Connect(Port_c *Port)
    \brief Connect to a Port_c on which buffers after manifestation are to be placed

    Calling this function indicates to the manifestor where it is to place decode buffers when
    their manifestation is complete. Though fairly innocuous looking, this is a critical function,
    after this is called the manifestor is expected to be in a running state, that is it should be
    ready to accept buffers for display, and other function calls.

    \param Port Pointer to a Port_c instance to take used buffers
    \return Manifestor status code, ManifestorNoError indicates success.
*/

/*! \fn ManifestorStatus_t Manifestor_c::GetSurfaceParameters(OutputSurfaceDescriptor_t      **SurfaceParameters)
    \brief Gain access to a manifestor maintained structure describing parameters of the output surface.

    The manifestor is responsible for maintaining a descriptor of the output surface parameters that
    may affect the behaviour of other components of the system such as the output timer. This function
    allows those other components to gain access to a pointer to that structure. The memory address associated
    with that pointer should remain valid until a call to reset().

    \param SurfaceParameters A pointer to a variable to hold a pointer to an OutputSurfaceDescriptor_t.
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

/*! \fn ManifestorStatus_t Manifestor_c::QueueDecodeBuffer(Buffer_t Buffer)
    \brief Queue a buffer for manifestation.

    Provide a buffer to the manifestor to allow manifestation of it's contents.

    \param Buffer A pointer to an instance of a decode buffer, as output
       from a codec.
    \return Manifestor status code, ManifestorNoError indicates success.
*/

/*! \fn ManifestorStatus_t Manifestor_c::QueueNullManifestation(void)
    \brief Insert blank, or silence, in the manifestation queue.

    Insertion is such that the screen will go blank, or the speaker
    will go silent, after the last queued buffer has been manifested.
    The null manifestation will terminate on the commencement of
    manifestation of a new buffer.

    \return Manifestor status code, ManifestorNoError indicates success.
*/

/*! \fn ManifestorStatus_t Manifestor_c::QueueEventSignal(PlayerEventRecord_t *Event)
    \brief Request that a manifestation event be emitted.

    A call to this function informs the manifestor that the specified event record
    is to be copied, and supplied to the Stream fn SignalEvent() at the appropriate time.

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

/*! \fn ManifestorStatus_t Manifestor_c::SynchronizeOutput( void )
    \brief Synchronize output so that any framing of data aligns with the current time

    This function largely applies to mechanisms with discrete frame outputs that are widely separated
    in time (ie video). It forces the frame boundary to allign (as closely as possible) withe the current
    time.

    \return Manifestor status code, ManifestorNoError indicates success.
*/
#endif

