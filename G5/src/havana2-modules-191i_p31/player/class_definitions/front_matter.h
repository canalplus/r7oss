/************************************************************************
Copyright (C) 2006, 2007 STMicroelectronics. All Rights Reserved.

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

Source file name : front_matter.h
Author :           Nick

A 'fake' header file containing only doxygen front-matter.

Date        Modification                                    Name
----        ------------                                    --------
21-Mar-07   Created                                         Daniel

************************************************************************/


//{{{  Main page
/*! \mainpage

\section intro_sec Introduction

Player 2 is a thorough redesign of the successful Player 1 architecture
currently deployed as the playback engine for digital video recorders
and media centres.

Player 2 revisits many of the design decisions of Player 1 modernizing
the architecture thus making is suitable for demanding applications
such as BD-ROM playback and A/V receiver while retaining the features
required to support traditional media centres.

Among the faults with player 1 are:

- There is one player 1 instantiation per playback (you may not have 
  noticed this if you have only one TV).
- Buffers carry no context (without fudging a header at the start).
- There is no way to flush instructions or operations down the decoding pipeline
- The avsync control is based around operations on frames of data
- There is no mechanism to import, or export the playback time and 
  consequently no way to synchronise external events with the playback.
- There is no way to dispose of a buffer out of line.

Player 2 addresses these and other issues, among the key 
changes to player 2 are:

- There is only one player instantiation, with multiple ongoing 
  playbacks.
- There is a more sophisticated buffer mechanism, based around a 
  class implementation, and supporting reference counts and meta data 
  attachment etc...
- Input, is via a buffer, and can be multiplexed or de-multiplexed. 
- It supports the passing of instructions/parameters down the buffer chain.
- Avsync is based around providing a preferred manifestation time 
  for every frame of audio/video, alongside an expected duration (to 
  allow video drivers, for example, to decide how long the top field of 
  an interlaced frame should be on display).
- Functions have been added to import and export time in its multiple 
  forms (See \ref time).

\section concepts_sec Internal concepts and data types

This section provides a conceptual overview of the Player 2
architecture.

- \ref buffers
- \ref demultiplexing
- \ref events
- \ref input
- \ref parameter_blocks
- \ref policies
- \ref playbacks_and_streams
- \ref process_model
- \ref specific_meta_data_types
- \ref time

\section developer_guide_sec Specific guidance for developers

- \ref adding_new_audio_codecs

\section pure_classes_sec Pure virtual classes index

The player's internal architecture is defined by the 
contractual behavior of a collection of pure virtual
classes. The implementation of a useful player consists
of three additional components.

- The player components, implementations of the pure
  virtual classes.
- The player wrapper, an interface by which implementations
  of the pure virtual classes can be instanciated and manipulated.
- Utility classes, used by the player components to help them
  implement their contractual behavior.

In order to understand any other aspect of the player a
thorough understanding the pure virtual class is implements
or manipulates is vital.

The most important pure virtual classes are listed below:

- BaseComponentClass_c: Base class from which all component classes are derived.

- BufferManager_c, BufferPool_c, Buffer_c: These form the 
  implementation of buffers as detailed in \ref buffers. 
  These wrap the buffers used throughout in this implementation. I 
  expect there will be only one generic implementation of these classes.
  Further I would expect there to be one instance of the buffer 
  manager, several buffer pool instances (at least 2 per individual 
  stream), and many buffer instances.

- Codec_c: This class will be responsible for decoding an individual 
  frame, and for passing codec parameters. This class has the least 
  change to its functionality in the new implementation, though due to 
  the buffer changes it will still undergo a change in its 
  implementation. I expect there will be an implementation of this 
  class for each decodable data format, and an instance for each 
  ongoing decode of that format.  

- Collator_c: This class is responsible for taking input data and 
  collating it into frames of input data appropriate for
  parsing/decode. This class will also be responsible for performing 
  any start code scan that is appropriate for the particular stream. I 
  expect there to be an implementation of this class for each decodable 
  data format, and an instance for each ongoing decode of that 
  format.

- Demultiplexor_c: This class is responsible for splitting input muxed 
  data into component streams and passing this data to the appropriate 
  collators. I expect there will at least one of these to support 
  transport stream demultiplexing, and possibly others for specific 
  muxed data streams. There is likely to be only one instance of each demultiplexor.

- FrameParser_c: This class is responsible for parsing an input data 
  frame, and submitting to the codec any appropriate decode requests. 
  This differs from the previous implementation in that there will be 
  no requirement to collate frames (as used to exist for audio), and no 
  requirement to handle multiple frames (as used to exist in some
  video, and audio formats). I expect there will be an implementation 
  of this class for each decodable data format, and an instance for 
  each ongoing decode of that format.

- Manifestor_c: This class represents the output mechanism of Player 2. 
  I have tried to extend the functionality of the manifestor, so that 
  it is capable of doing all that our current ones do (which is, as 
  with all things player 1, more than was originally intended), while 
  making it simpler. All things will now have a manifestation time as 
  opposed to the current play when you reach the head of the queue. 
  There will be data associated with the buffer to allow manifestation 
  mechanism to make wise decisions affecting manifestation. The class 
  will also have the concept of an abstract 'output surface' on which 
  to manifest the data, to allow external control of mixing or output 
  routing. I expect there will be an implementation of this class for 
  each decoded output type (audio, video, graphics ?), and an instance 
  for each ongoing decode of that type.

- Player_c: The glue that holds all the components. Its methods from
  the primary means for the player wrapper to manipulate a player
  instance.

- OutputTimer_c: This replaces old synchronizer, its primary function 
  is to generate output times, and expected durations, for decoded
  data, and in the case of speed changes to re-generate those times. It 
  also performs those adjustments relating to synchronization, basing 
  its adjustments on information about when a decoded frame arrived at 
  the manifestor later than its manifestation time (starvation), when a 
  decoded frame was removed from manifestation earlier than its 
  expected time (because the next frame arrived with an earlier 
  manifestation time), and what the number of filled coded frame 
  buffers is, when in real time playback. I expect there to be one 
  generic implementation of this, and an instance for each playback and 
  its associated streams.


*/
//}}}

/*
 * Internal concepts and datatypes pages
 */

//{{{  Buffer management
/*! \page buffers Buffer management

\section intro_sec Introduction

I have had a change of heart about buffers, in that I have decided that 
I would like them to be more in the fold so to speak. To that end I am 
making the buffer classes have additional player support functions. 
Specifically the RegisterPlayer() and SpecifySignalledEvents() calls 
need to be supported by the buffer pool. The reason behind this change 
is that it allows us the observe the release of buffers in a standard 
way from outside the player. In particular it allows us to use zero 
copy on input buffers, passing the memory in them into the bowels of 
the player, and being notified when they are released.

\section basic_ideas_sec The basic ideas

What I am looking for here is a way of allocating/managing buffers, 
and associating meta data structures with them. So what I think I need 
is something like a three level system, a manager level, a pool level, 
and an individual buffer level described below.

Both buffers and meta data have to be described in terms of their 
allocation mechanisms, allignement and size, but not their internal 
organization the buffer system assumes they are simply blocks of bytes.

Meta data types operate as both a type and an identifier, only one 
datum of a particular type can be attached to a buffer. If two things 
that have the same description need to be attached to a buffer, then 
they are described twice.

\section manager_level_sec The manager level 

Responsible for holding the definitions of buffers/data types, 
for creating/destroying pools of different sorts of buffers, and for 
dumping the entire buffer sub-system.

We start with no predefined buffer, or meta data, types, and have to 
define them before they can be used. The definition of a buffer, or 
meta data, type consists of a few simple pieces of information:

- <b>TypeName</b>, textual name (looks pretty on the dumps)
- <b>Type</b>, an identity for the type, an unsigned integer,
                can be a specified number, or an indicator that the
                definition function should create a new number.
- <b>AllocationSource</b>, Tells the manager where memory for this type is
                to be allocated from, covers from various malloc
                sources, to explicit attachment via a pointer.
- <b>RequiredAllignment</b>, Control to be applied to allocation.
- <b>AllocationUnitSize</b>, Control to be applied to allocation.
- <b>HasFixedSize</b>, indicates datum has a fixed size.
- <b>FixedSize</b>, The value of the fixed size.
- <b>AllocateOnPoolCreation</b>, for buffer data, indicates that it 
                should be allocated during pool creation. If set 
		buffers will each have one data block that is 
		associated with the buffer for the life of the pool.

When creating a buffer or meta data descriptor, the appropriate 
initialization macro should be used to set unused fields appropriately, 
and essential fields to detectable invalid values.

\section pool_level_sec The pool level

Responsible for containing a pool of buffers that all have the same 
description. Provides functions, to the user, to get a buffer and to 
attach, or detach, a meta data structure to all of the buffers in the pool.

In addition it provides a buffer release function that should be called 
by the individual buffer level class implementation, when it deduces 
there are no outstanding references to the buffer. Plus a dump function 
for the individual pool that can be called directly, or called by the 
manager level in persuit of a global dump.

\section individual_buffer_level_sec The individual buffer level

Individual buffer class, provides functions attach/detach and obtain a 
pointer to a meta data structure. Functions to shrink, expand or 
partition the data area in buffer (PLEASE NOTE data area expansion can 
be a costly exercise, involving data copying, and should be avoided in 
time critical code). A Function to Register a data buffer, for those
buffers using providing no AllocationSource.

In addition there are functions to increment and decrement the 
reference counts for the buffer. All buffers start with a reference 
count of one when obtained from the pool, when the reference count is 
zero. then the buffer is returned to the pool.

There are also functions, and parameters to functions, that enable 
the tracking of what (classes,etc) is/are referencing a buffer. The 
buffer class maintains a list of owner identifiers (just unsigned
int's), these are reported during a dump of the buffer's state as an 
aid to system debugging.

Further to these, there is also a mechanism to control release 
signalling via a function SetReleaseSignalling(), which passes pointers 
to both an OS abstracted event, and a boolean flag. The primary use for 
these is to allow control of allocation/deallocation of attached meta 
data, or registered buffer data. The use of both allows a single 
process to be awoken by the event, and then scan a list of boolean 
variables before deciding which buffers have been released. If release 
signalling is not set AFTER EACH call to get a buffer from the pool, no 
signalling, or setting, will be performed.

\section examples_sec Examples

\subsection buffer_ex1_sec Creating a 'user defined' meta data type

\subsubsection buffer_ex2_sec Part one: create the type

Derived from the source of manifestor_dummy.h, but changed
to create a metadata type rather than a buffer type

\code
#define METADATA_ERIC			"Eric"
#define METADATA_ERIC_TYPE       	{ METADATA_ERIC, MetaDataTypeBase, \
                                          AllocateFromOSMemory, 4, 0, true, \
					  false, sizeof(Eric_t) }

static BufferDataDescriptor_t            InitialDescriptor = METADATA_ERIC_TYPE;

BufferDataDescriptor_t                  *Descriptor;
BufferType_t                             Type;
BufferStatus_t                           Status;

//
// Find the type for eric if it already exists
//

Status      = BufferManager->FindBufferDataType( METADATA_ERIC, &Type );
if( Status != BufferNoError )
{

	//
	// It didn't already exist - create it
	//

	Status  = BufferManager->CreateBufferDataType( &InitialDescriptor, &Type );
	if( Status != BufferNoError )
	{
		report( severity_error, 
		        "Codec_MmeBase_c::InitializeDataType(%s) - "
			"Failed to create the %s buffer type.\n",
			Configuration.CodecName, InitialDescriptor.TypeName );
		return Status;
	}
}

//
// If you want to refer to the descriptor at a later date then retrieve it 
//
 
BufferManager->GetDescriptor( Type, MetaDataTypeBase, &Descriptor );
\endcode

\subsubsection buffer_ex3_sec Part two: attaching one to every item of a pool

\code
Status        = Pool->AttachMetaData( Type );
if( Status != BufferNoError )
{		    
	report( severity_error,
	        "Failed to attach Eric to all members of the pool.\n" );
	return Status;
}
\endcode

\subsubsection buffer_ex4_sec Part three: getting a pointer to the one attached to a specific buffer
         
\code
Eric_t      Eric;

Status	= Buffer->ObtainMetaDataReference( Type, (void **)(&Eric) );
if( Status != BufferNoError )
{
	report( severity_error,
	        "Failed to obtain pointer to meta data\"Eric\".\n" );
	return Status;
}
\endcode

*/
//}}}

//{{{  Demultiplexing
/*! \page demultiplexing Demultiplexing

In Player 1 de-multiplexing was originally incorporated to separate out 
the transport stream input into audio, and video, and to collate video 
into whole sequences before passing to the frame parsers. Later on in 
the life of Player 1, as the player expanded to cope with different 
stream types, it was enhanced to separate and collate different types 
of data, to scan for start codes and even to read pes headers and 
extract data from them. It has gone way beyond that for which it was 
originally intended, and now there are even cases where data streams 
are re-muxed before being passed to the demux. A further problem is 
that demux implementations cater for a stream containing both audio and 
video, and we have consequently duplicated functionality for audio 
types when paired with different video types.

In Player 2 we first need to recognise that many streams come
pre-demuxed, we also need to recognise that some collation/scanning 
function will continue to be needed by video, and audio, modules. 
Consequently the front end processing of data in Player 2 will consist 
of optional demux modules (depending on a MUX type), which will need to 
be registered with the player, to be available should an input be
muxed. Along with, for each playback stream, a collator class.

A Collator will take input data, of a defined format, and output, onto 
a ring, a sequence of collated frames of data. The Collator is 
responsible for both bunching the data into frames, and for performing 
start code scanning, with the results of such scanning being stored in 
meta data attached to the buffer.

A demultiplexor will be responsible for identifying which blocks of its 
input buffer belong to a particular stream, and making zero or more 
calls into the appropriate Collator for that stream with each of those 
blocks.

The Player 2 input mechanism will accept as input buffers, obtained 
from a pool, created by the BufferManager class. It will either pass 
the buffer whole to a demultiplexor module, for demultiplexing, or pass 
the body of the buffer directly into the appropriate Collator 
instantiation, depending on whether or not the buffer contains muxed 
data.

*/
//}}}

//{{{  Events
/*! \page events Events

We recognise that Player 1 today has no capability of exporting events 
to the outside world, relating to playout, specific times being
reached, particular conditions being encounterred, speed changes 
occurring, or any other useful occurrence. In order to rectify this I 
intend to put into Player 2 the capability to request specific events 
to be signalled internally to the Player, and for these to be retrieved 
via a single entrypoint into the player.

I expect there will be two basic classes of event, one-shot events 
where an action is requested, and a flag, or policy, indicates that an 
event should be signalled when that action completes, and ongoing
events, where a request is made to raise an event whenever something 
occurs.

What I expect is there will be two global entrypoints associated with 
event retrieval, one to register an OS abstracted event to be signalled 
when the event ring goes non-empty, and one to read an event record. An 
event record is likely to consist of an event identifier, playback and 
stream identifiers to tell you who generated the event record, a 
playback time for the event (this can be used to obtain a system time 
via the translate entrypoint to the player), a value (or pair of
values) associated with the event (likely to be specific to a 
particular event), and a void * user data optionally registered when 
you express an interest in the event. Something like:

\code
typedef struct PlayerEventRecord_s
{
    PlayerEventIdentifier_t        Code;
    PlayerPlayback_t               Playback;
    PlayerStream_t                 Stream;
    unsigned long long             PlaybackTime;

    union
    {
        unsigned int               UnsignedInt;
        UnsignedFixedPoint_t       FixedPoint;
        unsigned long long         LongLong;
        void                      *Pointer;
    } Value[4];

    void                          *UserData
} PlayerEventRecord_t;
\endcode

I do not expect there to a large number of events to be processed, this 
facility is for exceptional circumstances, not for signal me every 
frame style occurences.

The current list of possible event codes is something like:

\code
typedef enum
{
    // One-shot events
    EventFirstFrameManifested            = 0x00000001,
    EventPlaybackTerminated              = 0x00000002,
    EventStreamTerminated                = 0x00000004,
    EventStreamSwitched                  = 0x00000008,
    EventStreamDrained                   = 0x00000010,
    EventTimeNotification                = 0x00000020,
    EventDecodeBufferAvailable           = 0x00000040,

    // Ongoing events
    EventSizeChangeParse                 = 0x00010000,
    EventSourceSizeChangeManifest        = 0x00020000,
    EventOutputSizeChangeManifest        = 0x00040000,

    EventFrameRateChangeParse            = 0x00080000,
    EventSourceFrameRateChangeManifest   = 0x00100000,
    EventOutputFrameRateChangeManifest   = 0x00200000,

    EventBufferRelease                   = 0x00400000
} PlayerEventIdentifier_t;

typedef unsigned int             PlayerEventMask_t;
\endcode

Of these all but the last two are one-shot events. The parameter change 
events are raised (if requested) when a parameter change is noticed, at 
the parse stage, or at the manifestation stage, in both of these a mask 
will be used to indicate which changes are of interest (to be specified 
later), and the nature of the change along with the new value will be 
conveyed in the value encoded in the event record. 

*/
//}}}

//{{{  Input handling
/*! \page input Input handling

Data is input to the player via buffers, and can be multi-threaded. 
Input can consist of multiplexed streams, separate streams, or a 
combination of these (for 3 or more stream playbacks). 

The buffers to be input are no-allocation buffers, that is to say no 
data is allocated to them by the Player itself, data is attached to the 
buffers via the buffer <b>RegisterDataReference()</b> method. The 
buffers have associated with them a meta data structure describing the 
input, which must be initialized by the caller before injection. This 
structure is initially expected to look something like:

\code
typedef enum 
{
    MuxTypeUnMuxed           = 0,
    MuxTypeTransportStream,
    ... Any Others I can't think of ...
} PlayerInputMuxType_t;

typedef struct PlayerInputDescriptor_s
{
    PlayerInputMuxType_t    MuxType;
    DemultiplexorContext_t  DemultiplexorContext;
    PlayerStream_t          UnMuxedStream;

    bool                    PlaybackTimeValid;
    bool                    DecodeTimeValid;
    unsigned long long      PlaybackTime;
    unsigned long long      DecodeTime;

    unsigned int            DataSpecificFlags;

} PlayerInputDescriptor_t;
\endcode

If the data is multiplexed, the a <b>DemultiplexorContext</b> must be 
supplied, this is an opaque type managed by a demultiplexor. If the 
data is not multiplexed, the the variable <b>UnMuxedStream</b> must be 
set to the stream for which the data is destined.

The <b>DataSpecificFlags</b> is a value used to hold flags that are 
specific to the mux type, or collator type, they may include flags 
indicating frame starting in this block or other things that I am sure 
exist, but I don't know what they are yet.

It is expected that a significant portion of the input to Player 2 will 
be of the UnMuxed type. In order to better support the Muxed input
data, however, which may involve numerous small chunks, the input to 
the Collator classes will not be individual buffers, but will instead 
consist of blocks of 'unwrapped' memory, with a pointer to a
<b>PlayerInputDescriptor_t</b> structure, ie, something like:

\code
CollatorStatus_t   Input( PlayerInputDescriptor_t    *Descriptor,
                          unsigned int                Length,
                          unsigned char              *Data );
\endcode

Due to the handling of Muxed input, it is unsafe to assume that
<b>Data</b> will be aligned beyond the byte level.

\section input_stream_jumps_sec Input stream jumps

In trick mode injection it can be necessary to jump ahead or backwards 
in a stream or playback, at these times it is useful to indicate to the 
player that a jump is occurring. The Player needs to know which stream, 
or streams are jumping, and also whether or not there should be a 
discard of accumulated data, or a flush of any complete frames. In 
addition to this, the in the case of reverse   play, it is useful to 
indicate to the player that although there has been a jump, this is 
actually a reverse jump to the previous block of data (it is in fact a 
continuous reverse jump), since we may not be executing in reverse
play, we give this additional indicator a default value.

\code
PlayerStatus_t   InputJump( PlayerPlayback_t         Playback,
                            PlayerStream_t           Stream,
                            bool                     SurplusDataInjected,
                            bool                     ContinuousReverseJump  = false );
\endcode

One stream can be selected by supplying just a stream identifier, or 
several can be selected by supplying a playback identifier, and a
<b>PlayerAllStreams</b> value. 

The surplus data injected flag, if set, will cause the discard of any 
accumulated data in the collators, while if it is not set, the 
collators will be commanded to pass on accumulated data as  complete 
frames. 

The continuous reverse flag, if set, will ripple through the stream 
processing, allowing the decode of queued frames, and the release of no 
longer required reference frames.

*/
//}}}

//{{{  Parameter blocks
/*! \page parameter_blocks Parameter blocks

In the player 1 implementation, we always had the capability to set 
parameters to the classes by extending those classes with additional 
functions and calling them from user code. There was no way however to 
set parameters inline in the decode stream, all settings would be 
immediate. In player 2 we recognise that there may be occasions when 
parameters should be set for a module (collator, frame parser, codec, 
manifestor), and that these parameters should be capable of being in 
the flow of the current stream, or immediate. I have therefore added an 
entrypoint that allows the user to set a parameter block for a module, 
this parameter block can be inserted in the decode flow, or actioned 
immediately, and results in a call to the appropriate class instance, 
with the parameter block, at the appropriate time. A parameter block 
consists of a void * block of memory with a specified size.

*/
//}}}

//{{{  Playbacks and streams
/*! \page playbacks_and_streams Playbacks and streams

Since there is to be only one Player 2 running in the system, we need a 
playback hierarchy. A Playback, consists of one or more synchronized 
streams. In a multi TV box, or a box with a PIP capability, one 
playback may be one program comprising a video stream and an audio 
stream, while another may comprise only a video stream.

In Player 2 this means we have functions for managing playback creation 
and destruction, and that most of the Player2 calls apply to a specific 
playback, or a specific stream within a playback.

*/
//}}}

//{{{  Policies
/*! \page policies Policies

We identified in player 1 that there are certain policies that 
influence the behaviour, and user experience, of several components of 
the system, and that these policies may change at various points during 
the system's life. These started out as synchronization method
controls, and have evolved, and are expected to continue evolving, to 
other areas. Player 2 now has in its external interface a mechanism for 
setting policies, on various levels (everything, single playback, 
individual stream). 

Policies each have a small value, so pushbutton policies will have as 
the value one of several states, whereas boolean polocoes will only 
have two states indicating on, or off. The following are examples of 
some possible policy flags:

- <b>PolicyExternalTimeMappingOnly</b>, A boolean policy, this forces 
  the output time to only establish a mapping between system time and 
  playback time by use of the player SetNativePlaybackTime() function
  (which will invoke a call to EstablishTimeMapping() in the output 
  timer). 
- <b>PolicyRealTime</b>, A boolean policy, this affects whether, or
  not, the collators will wait for a buffer, should none be avaialable, 
  or discard the accumulated data. There are possible other effects for 
  this policy, that we will identify when using live feeds. 
- <b>PolicySynchronize</b>, A push Button, informs the
  <b>OutputTimer</b> of the preferred mechanism for synchronization 
  activity.
- <b>PolicyDisplayFirstVideoFrameEarly</b>, A boolean policy, rather 
  than wait until we have N frames before starting display, display the 
  first frame as soon as decoded, and leave it on screen while we 
  decode N frames.
- <b>PolicyTrickModeDecodePerField</b>, A boolean policy, informs the 
  Player, that it is desirable to use a different frame decode for each 
  field in >=2 speeds (IE in 2x, top field from frame N, bottom field 
  from frame N+1).
- <b>PolicyTrickModeDisplayFrameOnStartPaused</b>, A boolean policy, 
  informs the Player, that if a playback is paused, before the first 
  frame is displayed, then the first frame should still be displayed.
- <b>PolicyTrickModeAudio</b>, A push Button, informs the
  player/manifestor, what is to be done with audio when operating in 
  trick mode playback.
- <b>PolicyPlayoutOnTerminate</b>, A boolean policy, indicates that a 
  stream is to be played out when terminating a playback or stream.
- <b>PolicyPlayoutOnSwitch</b>, A boolean policy, indicates that a 
  stream is to be played out when switching a stream.
- <b>PolicyPlayoutOnDrain</b>, A boolean policy, indicates that a 
  stream is to be played out when draining a stream.
- <b>PolicyPlay24FPSVideoAt25FPS</b>, A boolean policy, indicates that 
  a playback is to be adjusted to play a 24 fps stream at 25 fps.

*/
//}}}

//{{{  Process model
/*! \page process_model Process model

The process model for Player 2 is somewhat different from player 1, in 
that I have added at least one process which should enable more 
concurrent operation. 

For each stream component to be decoded (video/audio data etc.) there 
are the following operations:

- <b>Caller Context</b>,
  All player entrypoints, including, on input, any demultiplexor, and 
  Collator effort.
- <b>Collator To Parser process</b>,
  Takes coded frame buffers, from the Collator's output ring, and 
  passing them onto the frame parser. The frame Parser which will, 
  after parsing is complete, pass the coded frame buffer, along with 
  the attached results of parsing, onto the parsed frame buffer ring.  
  Since the buffer is now a class instantiation, and can be returned to 
  the pool of available buffers by a call to one of its methods, this 
  task will no longer have to fudge the recording of when a buffer 
  shall become free.
- <b>Parser To Codec process</b>,
  Takes the parsed frame buffers from the Frame Parser's output
  ring, and passes them on to the codec to decode the frame. When the 
  codec completes decoding of the frame, the decode buffer will be 
  passed onto the Codec's output ring. Specific parameter meta data 
  will be copied from the frame buffer to the decode buffer, and the 
  frame buffer will be released.  Not every parsed frame buffer will 
  result in a completed decode transfer, since a frame may consist of, 
  in the case of video, a single field, or a single slice, or in the 
  case of audio, may generate no output.
- <b>Codec To Manifestor process</b>,
  Takes any completed decodes from the codec and submits them to 
  the manifestors. This process is also responsible for various other 
  operations, such as:
  - Waiting For frame information to be updated (see DIVX B frames)
  - Re-ordering decoded frames
  - Generating manifestation timing information
  - Synchronizing on speed change
- <b>Post Manifestation process</b>,
  This process is responsible for taking buffers from the Multiplexor, 
  and performing one of several functions on them:
  - Return frame to codec
  - Re-time frame and re-submit for manifestation

Of these the <b>Post Manifestation process</b> is perhaps the most 
different from its previous incarnation. Since we can now release a 
buffer via a call to one of the buffer's methods, it might be thought 
that there is no requirement for an explicit return to the codec. 
However we still require a context (other than an interrupt context) 
for the release to occur, and by invoking a codec method we enable a 
possible optimization for playback direction switch. In which the codec 
is able to re-use previously decoded, and manifested, frames to quickly 
switch playback direction.

NOTE: this is probably quite hard, and I am not 100% sure I wish to do 
it yet, but I definitely wish to keep my options open.

The Re-timing operation is required in the event of a speed change. In 
order to facilitate rapid switches in playback speed, we require the 
ability to retrieve frames that have been queued for manifestation with 
particular manifestation times, and to requeue them with times more 
appropriate for the new playback speed.

*/
//}}}

//{{{  Specific meta data types
/*! \page specific_meta_data_types Specific meta data types

This is my first stab at a set of named meta data types. Through the 
buffer manager we can control the attachment of meta data to buffers. 
However in order to identify meta data, and to use it correctly, it is 
important that the meta data type names, and the structure definitions 
are globally shared. Here we list the definition and names of some of 
the meta data types.

The input descriptor type, attached to input buffers.

\code
#define METADATA_PLAYER_INPUT_DESCRIPTOR    "PlayerInputDescriptor"
\endcode

For details of the structure definition for this type see \ref input.

The coded frame parameters, attached to a coded frame buffer, output from a collator. 

\code
#define METADATA_CODED_FRAME_PARAMETERS     "CodedFrameParameters"

typedef struct CodedFrameParameters_s
{
    bool                    StreamDiscontinuity;
    bool                    ContinuousReverseJump;
    bool                    PlaybackTimeValid;
    bool                    DecodeTimeValid;
    unsigned long long      PlaybackTime;
    unsigned long long      DecodeTime;

    unsigned int            DataSpecificFlags;
} CodedFrameParameters_t;
\endcode

The start code scan result, <b>optionally</b> attached to a coded frame 
buffer, output from a collator. 

\code
#define METADATA_START_CODE_LIST            "StartCodeList"

#define PackStartCode(Offset,Code)          ((unsigned int)(((Offset) << 8) | (Code)))

#define ExtractStartCodeOffset(Value)       ((unsigned int)((Value) >> 8))
#define ExtractStartCodeCode(Value)         ((unsigned int)((Value) & 0xff))

typedef struct StartCodeList_s
{
    unsigned int            NumberOfStartCodes;
    unsigned int            StartCodes[1];
} StartCodeList_t;

#define SizeofStartCodeList(Entries)        (sizeof(StartCodeList_t) + ((Entries-1) * sizeof(unsigned int)))
\endcode

The parsed frame parameters, attached to a coded frame buffer, output from a frame parser. 

\code
#define METADATA_PARSED_FRAME_PARAMETERS     "ParsedFrameParameters"

#define MAX_REFERENCE_FRAME_LISTS               2
#define MAX_ENTRIES_IN_REFERENCE_FRAME_LIST     16

typedef struct ReferenceFrameList_s
{
    unsigned int            EntryCount;
    unsigned int            EntryIndicies[MAX_ENTRIES_IN_REFERENCE_FRAME_LIST];
} ReferenceFrameList_t;

typedef struct ParsedFrameParameters_s
{
    unsigned int            DecodeFrameIndex;
    unsigned int            DisplayFrameIndex;
    unsigned long long      NormalizedPlaybackTime;
    unsigned long long      NormalizedDecodeTime;

    unsigned int            DataOffset;
    bool                    KeyFrame;
    bool                    ReferenceFrame;
    unsigned int            NumberOfReferenceFrameLists;
    ReferenceFrameList_t    ReferenceFrameList[MAX_REFERENCE_FRAME_LISTS];

    bool                    NewStreamParameters;
    unsigned int            SizeofStreamParameterStructure;
    void                   *StreamParameterStructure;

    bool                    NewFrameParameters;
    unsigned int            SizeofFrameParameterStructure;
    void                   *FrameParameterStructure;
} ParsedFrameParameters_t;
\endcode

The parsed parameters specific to Video, attached to a coded frame 
buffer for video frames, output from a frame parser.

<b>NOTE This is a very early stab, derived from the player 1 
manifestation parameters structure it is likely to change.</b>

\code
#define METADATA_PARSED_VIDEO_PARAMETERS     "ParsedVideoParameters"

#define MAX_PAN_SCAN_VALUES    3             // H264 has max seen value

typedef enum
{
    StructureTopField   = 1,
    StructureBottomField,
    StructureFrame
} PictureStructure_t;

typedef struct VideoDisplayParameters_s
{
    unsigned int                Width;
    unsigned int                Height;
    unsigned int                DisplayWidth;
    unsigned int                DisplayHeight;
    bool                        Progressive;
    bool                        OverscanAppropriate;
    UnsignedFixedPoint_t        PixelAspectRatio;
    UnsignedFixedPoint_t        FrameRate;
} VideoDisplayParameters_t;

typedef struct PanScan_s
{
    unsigned int                Count;
    unsigned int                DisplayCount[MAX_PAN_SCAN_VALUES];
    int                         HorizontalOffset[MAX_PAN_SCAN_VALUES];
    int                         VerticalOffset[MAX_PAN_SCAN_VALUES];
} PanScan_t;

typedef struct ParsedVideoParameters_s
{
    VideoDisplayParameters_t    Content;
    PictureStructure_t          PictureStructure;
    bool                        InterlacedFrame;
    bool                        TopFieldFirst;     // Interlaced frames only 
    unsigned int                DisplayCount[2];
    PanScan_t                   PanScan;
} ParsedVideoParameters_t;
\endcode

Note the overscan appropriate flag, indicates whether or not it is acceptable to lose pixels on the display. If this is not set, then all of the image must be displayed (no cutout center or otherwise).

The parsed parameters specific to Audio, attached to a coded frame 
buffer for audio frames, output from a frame parser.

<b>NOTE This is a very early stab it is likely to change.</b>

\code
#define METADATA_PARSED_AUDIO_PARAMETERS     "ParsedAudioParameters"

typedef struct AudioSurfaceParameters_s
{
    unsigned int                BitsPerSample;
    unsigned int                ChannelCount;
    unsigned int                SampleRateHz;
} AudioSurfaceParameters_t;

typedef struct ParsedAudioParameters_s
{
    AudioSurfaceParameters_t    Source;
    unsigned int                SampleCount;
} ParsedAudioParameters_t;
\endcode

The Output timing information specific to Video, attached to a coded 
frame buffer to control manifestation, output from the output timing 
module. Plus actual time values appended by the manifestation process.

\code
#define METADATA_VIDEO_OUTPUT_TIMING    "VideoOutputTiming"

typedef struct VideoOutputTiming_s
{
    unsigned long long          SystemPlaybackTime;
    unsigned long long          ExpectedDurationTime;
    unsigned long long          ActualSystemPlaybackTime;
    bool                        Interlaced;
    bool                        TopFieldFirst;     // Interlaced frames only 
    unsigned int                DisplayCount[2];     // Non-interlaced use [0] only
    PanScan_t                   PanScan;
} VideoOutputTiming_t;
\endcode

The Output timing information specific to Audio, attached to a coded 
frame buffer to control manifestation, output from the output timing 
module. Plus actual time values appended by the manifestation process. 

\code
#define METADATA_AUDIO_OUTPUT_TIMING    "AudioOutputTiming"

typedef struct AudioOutputTiming_s
{
    unsigned long long          SystemPlaybackTime;
    unsigned long long          ExpectedDurationTime;
    unsigned long long          ActualSystemPlaybackTime;
} AudioOutputTiming_t;
\endcode

The Output surface descriptor specific to Video, there is one instance 
of this for a decode stream, attached (by reference) to all of the 
output buffers. This contains information allowing the codec to decide 
on possible decimation of the decode, and to allow the timing module to 
calculate field counts for the display. 

\code
typedef struct VideoOutputSurfaceDescriptor_s
{
    // Display information
    unsigned int                DisplayWidth;
    unsigned int                DisplayHeight;
    bool                        Progressive;
    UnsignedFixedPoint_t        FrameRate;
} VideoOutputSurfaceDescriptor_t;

//

#define METADATA_VIDEO_OUTPUT_SURFACE_DESCRIPTOR         "VideoOutputSurfaceDescriptor"
#define METADATA_VIDEO_OUTPUT_SURFACE_DESCRIPTOR_TYPE    {METADATA_VIDEO_OUTPUT_SURFACE_DESCRIPTOR, MetaDataTypeBase, AllocateFromOSMemory, 4, 0, true, false, sizeof(VideoOutputSurfaceDescriptor_t)}
\endcode

The Output surface descriptor specific to Audio, there is one instance 
of this for a decode stream, attached (by reference) to all of the 
output buffers. This contains information allowing the codec to decide 
on possible application of sample rate conversion and down/upsampling. 

\code
typedef struct AudioOutputSurfaceDescriptor_s
{
    unsigned int                BitsPerSample;
    unsigned int                ChannelCount;
    unsigned int                SampleRateHz;
} AudioOutputSurfaceDescriptor_t;

//

#define METADATA_AUDIO_OUTPUT_SURFACE_DESCRIPTOR        "AudioOutputSurfaceDescriptor"
#define METADATA_AUDIO_OUTPUT_SURFACE_DESCRIPTOR_TYPE   {METADATA_AUDIO_OUTPUT_SURFACE_DESCRIPTOR, MetaDataTypeBase, AllocateFromOSMemory, 4, 0, true, false, sizeof(AudioOutputSurfaceDescriptor_t)}
\endcode

*/
//}}}

//{{{  Time management
/*! \page time Time management

\section time_domains_sec    Time domains

	Player 2 operates with three time domains :-

	- Native Playback Time, 
		This is the time as handled by a particular playback, 
		its format is dependent on the format of the coded data 
		(in mpeg2 this is PTS time). It is often based at some 
		arbitrary value (IE a stream may start at 0xdeadbeef).

	- Normalized Playback Time, 
		This is a scaled version of the native playback time 
		such that a delta of one in this scheme is equivalent 
		to an elapsed time of 1 us. It is also based at an 
		arbitrary value, derived by the scaling of the base 
		Native Playback Time.

	- System Time, 
		This is the abstract (OS inline) system clock, which also 
		runs at a 1us tick rate.

	These times are freely translated between in the player, the 
	playback times will have standard rules for translation that 
	are defined by the coded data content. The translation between 
	playback time and system time will be done via a mapping held in 
	the Output Coordinator. The mapping will be held across all the 
	streams that make up a playback (that is all the streams that 
	require to be synchronized together). In player 2 the mapping 
	between system and playback time can be established in one of 
	two ways. A mapping can be imposed from outside the player, or, 
	in the more common usage, a mapping can be derived by starting 
	playback of the component streams at the earliest possible time.

	Where streams contain jumps in PTSs the player will allow different 
	streams to transition from one mapping to a new one in a phased 
	manner. to allow seamless transition between clips.


\section clocks_sec    Clocks

	This is possibly the most confusing part.

	In the universe there are a large number of clocks, where 
	a clock is derived from another, then the second is actually 
	an expression of the first, and there is only one clock.
	No two clocks ever run at the same rate, there is a myth that
	atomic clocks do not drift with respect to each other, this 
	is only true if they are not in motion relative to each other, 
	and they sit at exactly the same point in the gravitational 
	topology of the universe (which effectively means that they 
	are one clock). In any system where there are multiple clocks
	none of them will be 'right', we have to choose one that we 
	will treat as being right.

\section clock_pulling_sec    Clock pulling

	In the player 2 model of the universe there is one system clock 
	plus an output clock on each stream. In a playback we choose one 
	of these clocks to be master, then we have two choices, we can 
	rate adjust the others (effectively locking them together), 
	so that we avoid frame repeats for audio/video or we can not 
	rate adjust and we accept frame repeat/drop for the non-master 
	output streams.

	Rate adjustment, for output clocks, involves actual adjustments 
	to the clock rate, for the system clock, involves a calculated 
	rate adjustment that is applied to temporal calculations, it 
	does not involve any physical adjustment to the system clock. 

	if we choose a good master clock then the rate adjustments for 
	the other outputs will be minimal, for example in the M3 release 
	we apply a policy that the video output clock is master, the 
	resultant calculations show that on my development board, we 
	need to adjust the audio output rate by approximately 2ppm. The 
	system clock is adjusted by a factor closer to 47ppm.

	In practice, audio output may have several clocks one for each 
	output method, but these are effectively locked together by the 
	output driver.

	Also in practice for a single video output display, there is only 
	one video output clock, so for completely independent output streams
	(non-synchronized PIP) all video outputs bar one (to this display) 
	must have video master set, or must allow frame repeat/drop.

	The source code for the clock rate adjustment values is appended 
	at the bottom of this file, it involves integrating the actual 
	and expected duration of frames (rejecting large jittered values)
	and calculating a rate adjustments based on those values.


\section synchronization_sec A/V/D synchronization

	AVD synchronization has three major components

	- Startup synchronization

	- Ongoing output rate adjustment as described above

	- Percussive corrections

	For startup synchronization we wait (for a limited time) for each
	component stream within a playvback to have decoded sufficient 
	frames to playback continuously. (in the case of audio this is 
	sufficient to allow the manifestor not to starve, in the case of video
	it is sufficient to have in hand the appropriate decoded reference 
	frames). We then compare the playback times of the frames, with the 
	time until their respective manifestors can display them and set a 
	base system time and a base playback time and instruct the manifestors
	to commence playing at the appropriate times. For many streams 
	playback commencement will not be at the same actual time since 
	in transport streams audio and video may be offset within the stream.

	The ongoing rate adjustment is as described before via clock pulling,
	and is responsible for correcting drifts in the synchronization on 
	the several parts per million scale.

	Percussive corrections are responsible for handling large 
	synchronization errors they are for knocking the streams back into 
	synchronization. The algorithm applied to detect the need for 
	percussive synchronization is a simple integration scheme over a
	number of frames, the sync must be out by more than a threshold
	value for all of the frames before correction will be deployed. 
	Currently we are integrating over 8 frames and the threshold is 
	1ms.

	Percusssive corrections are applied to video by dropping/extending 
	frames. They are applied to audio by informing the audio driver 
	that a frame of N samples is to be played over M sample periods, 
	the audio driver is at liberty to achieve this by any means desired.

	Startup synchronization will be deployed when streams start, and 
	when they skip, and in any other circumstance where a stream drains.
	Ongoing rate adjustment is expected to be perfomed at all times. 
	Percussive corrections will occur if the startup synchronization 
	is imperfect (currently usually), if there is corruption or stream 
	dropount (test streams) and every 10 minutes or so if output rate 
	adjustment is disabled. If playing a blu-ray DVD with output rate 
	adjustment enabled, percussive adjustment is not expected to occur 
	beyond the startup phase.

	In the current M3 release, we are seeing a startup synchronization 
	error of upto 40ms which is corrected after some 250ms after which 
	we remain within 1ms until after some 10 minutes when the fact that 
	output rate adjustment is disabled for audio causes a 1 ms correction.
	We expect to bring the startup error down to within 8ms later in the 
	project, and to use output rate adjustment to avoid regular kicks.
	We do not expect to reduce the 250ms period for tightening the sync
	to sub 1ms, since this is dependent on the integration period used 
	to detect the startup error (it would be possible to halve it by 
	reducing the integration period, however this may lead to a second 
	order correction of a smaller magnitude later).

	Special cases, we anticipate deploying percusive corrections to 
	audio in two additional, related, circumstances :-
	NOTE not all customers may be interested in these mechanisms :-

	- When playing a stream at +20% speed (certain customers require 
	  the ability to play recoded movies at +20% speed with audio pitch 
	  correction, because they cannot find the time to watch a movie at 
	  normal speed). In this circumstance the correction on every audio 
	  frame will be coded as if it were a percussive correction.

	- When playing a movie at 25/24ths normal speed, ie when taking a 
	  24fps movie and playing it at 25fps (again some customers require 
	  the ability to play 24fps content on a 50hz display without frame 
	  repeat).


\section clock_pulling_scenarios_sec    Clock pulling scenarios - the real world

	Give the clock pulling descriptions above, it is probably worth 
	detailing certain scenarios, and recommended settings :-

	- Case 1 - simple blu-ray playback

		Here we select the video clock as master,
		the audio clock is then locked to it via 
		output rate adjustment.

	- Case 2 - 2 separate blue-ray playbacks with PIP

		Here we select the video clock as master for 
		both playbacks.

	- Case 3 - Avr playback with clock matching

		Here we set the system clock as master, and 
		timestamp incoming frames from the system clock,
		both audio and video are independently rate 
		adjusted to match the rate of the incoming frames.

	- case 4 - Avr playback without clock matching

		As case 3, but with the streams not requiring 
		clock matching having clock matching disabled 
		(this is a flag that will need to be implemented
		for AVR). Corrections to the affected streams 
		will then be applied by percusssive av 
		synchronization, these will deploy automatically.

	- case 5 - Avr playback with clock matching 
		   plus blu-ray.

		This is achieved as a straight copy of case 3 
		and case 1, the avr stream adjusts the video output
		clock to match its' input and the blu-ray output 
		simply matches to that. For mixed audio output 
		there will have to be sample rate conversion of the 
		blu-ray stream to that of the avr stream.

	- case 6 - Avr playback without clock matching 
		   plus blu-ray.

		This is a complicated case dependent on which 
		streams have clock matching and whether or not there
		is mixing of the input. You ask about the specific 
		case and we will will figure out how it works.

	I believe that as part of the AVR implementation, there will 
	have to be a rebase mechanism for the output rate adjustmnents, 
	to be applied when policy configurations are changed. Specifically
	we will need to know what to do when we have been using clock 
	matching on an AVR stream in a PIP window, when we remove the 
	AVR but continue on with the blu-ray presentation, do we :-

		A/ Stay as video master, but with video set 
		   as it was to match the input avr stream

		B/ Stay as video master, but reset the output rate 
		   to what it would have been if the AFVR input had 
		   never been connected.

	This will have to be a customer decision.


\section video_frame_rate_conversion_sec    Video frame rate conversion

	Frame rate conversion comes in two areas, frame rate conversion for
	standard blu-ray playback, and for AVR playback. In both cases the 
	algorithm is the same, we have source frame rate suplied to the 
	player, either externally in the case of AVR (and this may involve 
	an integrating filter to differentiate 59.xxx and 60 hz frame rates), 
	or via the frame parser in the case of coded data. We have 3:2 
	pulldown and speed effects (both of which will be absent from 
	the AVR), and we have an output frame rate. In addition to these 
	we also have pan scan vectors for coded frames. 

	The source code for our function to fill out a frame timing record 
	is appended below, but a brief abridged desription is :-

	- Input Nature of the input (field/frame counts, source frame 
	  rate etc...), plus time at which the frame should be displayed
	  ignoring frame rate conversion 

	- Check the incoming frame rate and limit to a sensible value

	- Check for 3:2 pulldown, if we have 60hz => 50hz that was coded
	  with 3:2 pulldown remove it, and correct the display time.

	- If any multiplier parameters have changed re-calculate the 
	  conversion multiplier (using rational arithmetic)

	- Adjust the display time to compensate for the running error
	  (will be zero if no frame rate conversion)

	- Calculate new field counts using the lines of code :-

	<pre>
	FrameCount                          = (CountMultiplier * ParsedVideoParameters->DisplayCount[0]) + AccumulatedError;
    	VideoOutputTiming->DisplayCount[0]  = FrameCount.IntegerPart();
	AccumulatedError                    = FrameCount.Remainder();
	</pre>

	- Calculate rate converted pan scan counts

	- Apply any ongoing percussive video sync correction to
	  the rate converted counts.

	- Store the outputs in the output timing record

	- If running in reverse with an interlaced frame switch the 
	  field order of counts and pan scan vectors.

	I guess the crucial part is that we obtain a simple multiplier 
	for frame rate conversion held as a mrational number, and that 
	we multiply field/frame counts by the multiplier, maintaining 
	an acumulated frame error value.

	Issues yet to be considered clearly when we are not displaying 
	at the native display rate for a stream, there will be issues 
	with any meta data associated with frames which can be lost,
	or may need extension. The code already handles pan/scan vectors 
	passing through frame rate conversion, but a more difficult issue 
	is likely to be VBI or Closed Caption data.

	In general frame rate conversion, of captured content, from 
	60 => 50hz risks being messy, if the 60hz content was generated 
	with 3:2 pulldown, we are unable to remove the 3:2 pulldown in 
	the frame rate conversion, dropping frames/fields is unlikely 
	to match the extended fields/frames inserted in the content so
	the output is unlikely to look good (of course there is the question 
	of why would we want to output at 50hz in a box such as this).

\section obsolete_sec Obsolete

This section contains out of date information regarding the player's handling
time. However the replacement test (above) was circulated with the following proviso.

<pre>
  Looking thorugh my mails, I think that they do not summarise the 
  position, in one chunk, coherently. So I am going to try to write a short 
  summary of all the interesting bits about time in one place as it applies 
  to a player 2 based system. This may confuse more than enlighten, in which 
  case ignore it and read mails instead.
</pre>

For that reason this section cannot be deleted for the time being.

\todo This section can probably be deleted but the section needs to be
      reviewed by Nick Haydock (who authored both the old and the new text).

<b>Time is an illusion, player times doubly so !!!</b>

There are three different (but related) time domains in any playback.

- <b>Native Playback Time</b>,
  This is the time as handled by a particular playback, its format is 
  dependent on the format of the coded data (in mpeg2 this is PTS
  time). It is often based at some arbitrary value (IE a stream may 
  start at 0xdeadbeef). 
- <b>Normalized Playback Time</b>,
  This is a scaled version of the native playback time such that a 
  delta of one in this scheme is equivalent to an elapsed time of 1 us. 
  It is also based at an arbitrary value, derived by the scaling of the 
  base Native Playback Time.
- <b>System Time</b>,
  This is the abstract (OS inline) system clock, which also runs at a 
  1us tick rate.

The Native Playback time is important because it is the streams native 
time, and it effectively encodes all the information necessary to 
perform audio/video and any related application synchronization. 
Normalized Playback Time encapsulates the same information as Native 
Playback time, whilst scaling in a way that can be manipulated by 
components that are not tied to the particular stream being played (ie 
we don't want to have an mpeg2 player, synchronizer and manifestor 
where the only relevent difference between mpeg2 and divx is the rate 
at which time ticks). The Normalized Playback Time retains the 
arbitrary base, since it is the sharing of that base by all the 
components of a playback that allows synchronization between them. The 
System Time is used to relate events to the system clock.

The domains interact as follows:

- A frame (audio or video) is parsed and the Native Playback time for 
  it is extracted or calculated.
- The Native Playback time is converted to a Normalized Playback Time 
  in US, and associated with the frame as it passes through the code to 
  be decoded.
- The Normalized Playback Time for a frame is then converted to a 
  System time by the synchronizer and the frame is passed to the 
  manifestor, which manifests the frame at the appropriate time.

Player 2 allows several simple operations that control the use of time, 
and the translation between its domains. These funtions allow the 
import and export of synchronization information from the player.

- <b>SetPlaybackSpeed</b>,
  This causes playback time to flow at a faster or slower rate, and 
  affects both the duration of frames, and the translation applied by 
  the synchronizer.
- <b>SetNativePlaybackTime</b>,
  This allows an external synchronizer to drive player synchronization. 
  This allows an external synchronization module to inform the 
  synchronizer what it thinks is the current native playback time.  
  This will be translated by one of the frame parsers to the normalized 
  US based format. This controls the translation, by the synchronizer, 
  of Playback time to System time.
- <b>RetrieveNativePlaybackTime</b>,
  This allows an external synchronizer to be driven by player 
  synchronization. It allows an external synchronization module to ask 
  the synchronizer what it thinks is the current playback time. It will 
  be translated by one of the frame parsers to native playback related 
  format.
- <b>TranslateNativePlaybackTime</b>,
  This allows an external synchronizer to to discover at what system 
  time it should manifest its data. It translates a Native playback 
  time to a system time.

In addition to these, the synchronizer has the following function

- <b>AdjustSystemClockRate</b>
  This adjusts the rate at which the synchronizer expects the system 
  clock to tick. This is used by external clock recovery drivers when 
  there is no hardware mechanism to adjust the system clock rate. It 
  affects the translation of playback time to system clock time.

*/
//}}}

/*
 * Developers Guide
 */

//{{{  Adding new audio codecs to the player
/*! \page adding_new_audio_codecs Adding new audio codecs to the player

\section audio_introduction_sec Introduction

This section provides a high level overview of the MPEG audio support 
included in the player and describes how to use the MPEG audio code as
an example from which other audio codecs can be written.

There are, in fact, three classes that must written in order to support
a specific audio codec.

 - A collator, which is responsible for identifying frame boundaries.
 - A frame parser, which is responsible for extracting any information
   contained in the frame header that is required to configure the
   decode.
 - A codec (proxy), which is responsible for arranging that the decode
   take place.

There are also a small number of additional tasks to construct a complete
system, such as defining applicable data types and registering factories
to instanciate the new classes.

\section audio_headers_sec Headers

Before attempting to implement the three classes required to support a 
new codec it is worthwhile defining the shared data types the classes 
will use to communicate codec specific information to each other. This 
also offers the chance to define constants and other standardized data
regarding the encoding (e.g. frame structure, PES start codes).

mpeg_audio.h is a good example of a header file that defines the data
types and constants.

This file consists of:

 - A collection of bitmasks describing the structure of the frame header.
 - A data structure, ::MpegAudioParsedFrameHeader_t, used to describe the
   'exploded' frame header.
 - A data structure, ::MpegAudioStreamParameters_t, used to describe
   stream parameters and the associated buffer descriptor,
   #BUFFER_MPEG_AUDIO_STREAM_PARAMETERS_TYPE. The stream parameters are
   typically those parameters that ultimately map to
   MME_CommandCode_t::MME_SET_GLOBAL_TRANSFORM_PARAMS commands.
 - A data structure, ::MpegAudioFrameParameters_t, used to describe
   frame parameters and the associated buffer descriptor,
   #BUFFER_MPEG_AUDIO_FRAME_PARAMETERS_TYPE. These parameters are
   those that will either be sent to the decoder within
   MME_Command_t::Param_p or those that will be used to verify that
   MME_CommandStatus_t::AdditionalInfo_p is correct.

\section audio_collator_sec Collator

The collator is responsible for consuming encapsulated data, removing the encapsulation
and emiting the resultant elementary stream on strict frame boundaries.

All data provided to the player is packed into PES packets.
If PES is not the native
format for the data the userspace program is responsible for repackaging the data into PES
packets.
The shared super-class for all audio collators, Collator_PesAudio_c is
repsonsible for removing the encapsulation. In order to do this it must be appropriately
configured; the sub-class is therefore required to override the Collator_PesAudio_c::Reset() method
in order to provide the appropriate configuration. See Collator_PesAudioMpeg_c::Reset() for
an example of this.

Once the encapsulation has been removed the data is passed to
Collator_PesAudio_c::HandleElementaryStream(). This method, and its helpers, are responsible
for locating the appropriate synchronization sequence within the bitstream. The method
implements the traditonal techinique of performing partial frame analysis at the point the
synchronization sequence is located and calculating when the next synchronization sequence
will be located. The data is only declared 'good' if the next synchronization sequence is
observed. To correctly handle the last frame of a sequence the player directly calls
Collator_c::FrameFlush() which will cause a frame to emited without observing the subsequent
synchronization sequence.

This algorithm is embedded in Collator_PesAudio_c and the sub-class need only override three
helper methods to accurately collate data, Collator_PesAudio_c::FindNextSyncWord(),
Collator_PesAudio_c::DecideCollatorNextStateAndGetLength() and 
Collator_PesAudio_c::SetPesPrivateDataLength().
See 
Collator_PesAudioMpeg_c::FindNextSyncWord(),
Collator_PesAudioMpeg_c::DecideCollatorNextStateAndGetLength(),
Collator_PesAudioEAc3_c::DecideCollatorNextStateAndGetLength and
Collator_PesAudioEAc3_c::SetPesPrivateDataLength()
for example implementations.

Note the way the frame length is calculated using the static method
FrameParser_AudioMpeg_c::ParseFrameHeader() to avoid the duplication of 
code between the collator and the frame parser. Following this model, while
optional is highly recommended.
The Collator_PesAudio_c::DecideCollatorNextStateAndGetLength() plays a fundamental role
in switching between collator states. The state switch decision must be taken according to the 
previous state and the nature of the sub-frame encountered. According to the codec type, the collator 
state might be changed to:
   - GotCompleteFrame (an end of the frame is detected, so the frame can be sent to the frame parser)
   - SkipSubFrame (the detected subframe does not need to be sent to the frame parser)
   - ReadSubFrame (the detected subframe is to be sent to the frame parser)
In any case the state MUST be changed otherwise the collator may be lost. 

The role of the SetPesPrivateDataLength() method is to specify, according to the stream_id of the PES,
the private data area size within the packet according to the codec type. Collators can choose whether
or not the private data area is to be considered part of the elementary stream using the boolean,
Collator_PesAudio_c::PassPesPrivateDataToElementaryStreamHandle. For most encoding the PES private data
area, if it exists does not form part of the elementary stream, however encoding such as DVD-video LPCM
might be implemented differently. Also for some encodings, included
AC3, the private data area is optional (i.e. it may or may not be present) and the collator must
also determine whether the private data area is present. The super-class provides a protected virtual
method, Collator_PesAudio_c::HandlePesPrivateData() to accomadate this requirement, see
Collator_PesAudioEAc3_c::HandlePesPrivateData() for an example.

\section audio_frame_parser_sec Frame parser

The frame parser framework is, to a much greater extent than the collation framework,
shared between audio and video. This means that the implemention of an audio frame parser
uses a far greater number of methods. Many of these methods are concerned
with the management of reference frames which has little relavence to the vast
majority of audio codecs. As can be seen from examining the implementation of
FrameParser_AudioMpeg_c there are a large number methods without meaningful implementation
(i.e. they simply return success). For this reason not all methods a sub-class is required
to implement are enumerated in this section, instead we cover only the two most vital. It is
recommended that FrameParser_AudioMpeg_c be used as a template for any method not discussed
below.

The first method, FrameParser_Base_c::ReadHeader() is used to extract 
the information held in the frame header, allocate storage for these 
parameters and to copy then into that storage. The method is not 
responsible for passing the parameters onward, this is performed by the 
super-class. The sub-class need only concern itself with setting up 
data members pointing to the parameters. Example code can be found in
FrameParser_AudioMpeg_c::ReadHeader().

The second method, FrameParser_Base_c::GeneratePostDecodeParameterSettings(),
is used to generate the parameters that will be attached to the data produced by
the decoder. The parameters can be generated immediately, or, if there is
insufficient information available to determine these parameters, generation
can be deffered (for MPEG audio the post decode parameters can always be
generated). The post decode parameters settings \b must include the
(normalized) presentation time stamp of the frame (although it is possible for
the codec proxy to attach this information post-hoc). If this information cannot
be obtained directly from the encapsulation layer then its value must be
inferred. If inference is impossible, typically when no PTS have ever been observed
in the stream, then the frame must be discarded since the manifestor will be unable
to render it. Example code can be found in
FrameParser_AudioMpeg_c::GeneratePostDecodeParameterSettings(). Note the
carefully tracking of the accumulated error in the inferred PTS values. This
is especially important for 44.1KHz sampling frequencies.

\section audio_codec_proxy_sec Codec proxy

The final component that must be created is the codec proxy. This section
is writen assuming that the audio codec is realized as an MME transformer
and describes only the framework supporting MME based codec proxies.

Like the collator framework the sub-class to support a specific audio
codec is relatively simple.

Firstly there are a family of similar FillOut... methods that must be overriden,
Codec_MmeAudio_c::FillOutTransformerGlobalParameters(),
Codec_MmeBase_c::FillOutTransformerInitializationParameters(),
Codec_MmeBase_c::FillOutSetStreamParametersCommand() and
Codec_MmeBase_c::FillOutDecodeCommand(). Each of these is responsible
for populating the structure identified in the method name. Note that the
structure of the ACC firmware's parameters results in a method specific to audio, 
Codec_MmeAudio_c::FillOutTransformerGlobalParameters(), which is, in
effect, a utility class used to fill out both the initialization parameters
and the set stream parameters calls.

Another method that must be overriden is Codec_MmeBase_c::ValidateDecodeContext().
This is called from within the MME callback and has the dual purpose
of squawking on the console if errors are detected (the firmware's tendancy
to soft mute on error, while useful, does mean that the some decode errors
may go unnoticed without console output) and of filling in any post decode
parameters that the frame analyser was unable to discover. For most of the
encodings the frame analyser is fully capable of populating the post decode
parameters. One obvious exception is WMA, whose frame structure can not be easily
parsed by the host processor since its frame structure is huffman encoded along 
with everything else. In this case almost all the post decode 
parameters must be supplied by the codec proxy.

Examples for all of the above methods can be found in Codec_MmeAudioMpeg_c.

The final pair of methods, Codec_MmeBase_c::DumpSetStreamParameters() and
Codec_MmeBase_c::DumpDecodeParameters() are optional. These are used purely
for debug and are intended to show the parameters supplied to the decoder.

The ACC firmware's parameters are very complex and these methods should ideally
be implemented generically as part of Codec_MmeAudio_c.

The MPEG audio codec proxy does not implement these methods, however examples
can be found in Codec_MmeVideoMpeg2_c::DumpSetStreamParameters() and
Codec_MmeVideoMpeg2_c::DumpDecodeParameters().

\section audio_factory_functions Player wrapper factory functions and LinuxDVB support

At this point all the code is written to handle the new audio codec but the code
will never be instanciated because the LinuxDVB and player wrapper layers have not
been made aware
of it.

There are two changes that must be made to register the new codec with 
the player wrapper.

Firstly add an macro to backend_ops.h to describe the new codec. Use #BACKEND_MPEG2_ID
as an example (though try to ignore that fact that this code is also used for video).

Secondly register a factory function using the new backend identifier. The function that
registers all in-built collators, frame parsers and codecs is ::RegisterBuiltInFactories(),
this function should be expanded to register appropriate factory functions, which must
be written. The existing factory functions, ::CollatorPesAudioMpegFactory(),
::FrameParserAudioMpegFactory() and ::CodecMMEAudioMpegFactory(), serve as example code.

Once registered with the player wrapper the new codec now needs to be supported by LinuxDVB.
Again there are two changes that must be made.

Firstly extend the ::audio_encoding_t enumeration to include an constant to describe
the new codec. Insert this at the end. This may look slightly illogical byt will
preserve binary compatiblity we previous releases.

Secondly extend the ::AudioContent array, which is indexed by ::audio_encoding_t, to
tie the LinuxDVB name AUDIO_ENCODING_XXX to the player wrapper name BACKEND_XXX_ID.
Obviously because the array is indexed by ::audio_encoding_t the ordering is
critical if the meaning is to be preserved.

*/
//}}}

