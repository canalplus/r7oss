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
Author :           Daniel

A 'fake' header file containing only documentation on the
audio manifestor architecture.

Date        Modification                                    Name
----        ------------                                    --------
23-May-07   Created                                         Daniel

************************************************************************/

/*! \page audio_manifestor_micro_architecture Audio manifestor micro architecture

\section intro_sec Introduction

This page describes the low-level design of Player2's
audio output system. The entire of this document focuses on the requirements of HD-DVD and BD-ROM playback. All other applications to
which the audio output system will be put, including
DVD, set top box, media centre and A/V receiver, are
all strict subsets of the requirements for BD-ROM
playback.

\section requirements_sec Requirements

The following inputs are presented to the manifestor
system all of which must be mixed.

 - Primary stream, delivered to an instance of Player2's Manifestor_c.
   This stream is time stamped and thus subject to synchronization.
 - Secondary stream, delivered to an instance of Player2's Manifestor_c.
   This stream is time stamped and while subject to
   synchronization should
   not have any sync. 'tension' versus the primary stream. In other words any output rate adjustment (a.k.a. clock pulling) applied to the primary stream is applicable to the secondary stream. If this invariant is violated then percussive adjustments are acceptable.
 - Up to 8 interactive monaural streams, delivered as LPCM from outside of 
   Player2 (via the ALSA playback interface). The final two monaural streams may be
   combined to form a single stereo stream. These streams are not time stamped nor subject to syncrhonization
   and must simply be presented with minimal latency.

The primary and secondary streams are continuous and
seeking through them is anticipated, thus any discontinuity in the primary and secondary streams must result in a de-click across the transitions.

The interactive streams are intended for incidental sound effects and other short running audio applications. These inputs are not subject to click suppression by the presentation system. It is the responsibility of the application driving ALSA to ensure that discontinuity and other transitions are handled smoothly.

Note: Some observed commercial BD-ROM titles
(ab)use the interactive streams for long running audio streams but this does not change the nature of the application's responsibility.

Once these streams have been mixed together they must be output on multiple physical outputs. The precise set of outputs will ultimately be dictated by the customer board. Typically these may include:

 - Analog DAC presenting a stereo PCM downmix.
 - Analog DAC presenting multi-channel PCM.
 - SPDIF either presenting a stereo PCM downmix or a re-encoded multi-channel compressed output.
 - HDMI either presenting stereo downmix, multi-channel PCM output or re-encoded multi-channel compressed output.

To allow a concrete design to be produced we define the following outputs as being 'Profile 1 (simple 5.1)'

 - 2 analog DACs each presenting a stereo PCM downmix (headphones + RCA).
 - SPDIF presenting a re-encoded multi-channel compressed output.
 - HDMI presenting a stereo PCM downmix.

For profile 1 the following simplifying assumptions are made:

 - The SPDIF re-encoding is limited to codecs supporting 5.1 speaker organisation.
 - All outputs are limited to no more than 48KHz.
 
For all profiles the audio output system must assume responsibility for preventing pops and clicks and that, for everything except alterations to the nominal output sample rate, this must be realized using software muting. This requirement stems from the relatively slow reaction of typical consumer amplifiers to a hard mute request.

\section quirks_sec Quirks

Where there is more than one way to achieve a technical goal some customers specifically favour one approach over another. This section describes specific known customer preferences that must be supported.

For profile 1 this include the following:

 - Some applications may require the output sample rate to be fixed at 48KHz. This implies the use of digital sample rate conversion in order to playback other samples rates (the most important of which is clearly 44.1KHz).
 - The term codec (as applied to the Profile 1 SPDIF output) is fairly broad and encompasses uncompressed PCM delivered using overclocked SPDIF links (e.g. 4x over-clocking).

\section future_sec Anticipated future requirements

Future profiles are likely to require:

 - Higher sampling frequencies (192KHz has already
      been mentioned by some potential customers). Such high sampling
      frequencies
      should not affect the mixer architecture but may strain the audio coprocessors
      somewhat. It is also likely to require much larger mixer granules to amortize
      communication overhead.
 - Support for 7.1 speaker organisation and cross mixing between different 7.1 speaker topologies.
 - Support different output topologies. In particular the provision of 8 channel PCM or 'HD' compressed data (e.g. DTS-HD, DD+) via HDMI.

\section mixer_overview_sec Mixer overview

The mix process is can be approximated to the following actions (in order):

 - Allocate playback buffers for each of the outputs,
 - Obtain descriptors for each of the input buffers,
 - Zero the mixer output buffer,
 - For each input buffer:
   - Sample rate convert each of the input buffers to the native mixer frequency and store in the SRC output buffer. For the primary stream this step is typically omitted since the input and output sample rates match.
   - Mix SRC output buffer (or original buffer) into the mixer output buffer.
 - Apply any shared PCM post processing that may be required (e.g. volume control),
 - For each output:
   - Apply any PCM post processing applicable to the current output (including secondary SRC if the output is incapable of presenting at the mixer's native frequency).
   - Optionally pass the PCM through an encoder (e.g. AC3).
   - Optionally pass the PCM through an SPDIF stream formatter to provide framing information.
 - Commit the playback buffers for presentation.

Note: Since only one mix can take place at once and the maximum number of output samples per mix iteration is known in advance there need only be one pre-allocated mixer output buffer and one pre-allocated SRC output buffer.

Note #2: Some forms of PCM post processing may require additional pre-allocated temporary buffers.

The idealized mix process
outlined above is split between the host processor and one of its ST231 co-processors.
Specifically the host manages of the input and playback buffers (and thus the low level PCM and SPDIF player drivers) while the co-processor manages all other aspects of the mix process, includng the (pre-)allocation of temporary buffers.
Specifically the host performs the first two steps of the mix process and then generates MME descriptors for all the buffers and orders the co-processor to perform a mix iteration using ::MME_SendCommand(). Once the mix iteration is complete the co-processor notifies the host which then commits the output buffers for playback and starts again.

Within Player2 the host portions of the mix processor are implemented in Mixer_Mme_c::PlaybackThread().

The co-processor portions are implemented by the MME_TRANSFORMER_TYPE_AUDIO_MIXER tranformer.

\section input_mgt_sec Input buffer management

The mixing of the primary and secondary streams operates on the principle that there will always be decoded
buffers available for it to consume (push model). It therefore consumes buffers
using a non-blocking dequeue.

It is an error condition for the buffer queue to
underflow. Nevertheless this must be handled gracefully. Gracefully, for these
purposes means entering a muted state without pops or clicks on
the speakers. This protection is achieved by always maintaining a short buffer
of unplayed samples that have already been dequeued. If the mixer input
underflows, these unplayed samples can be faded out delivering the system
into the (starving) muted state without speaker artifacts. This same mechanism is used when playback is terminated though in this instance starvation does not occur.

The mixing process is 'paced' by the \b consumption by the PCM and SPDIF players of mixed buffers. In other words whenever there is an output buffer available to be filled the mixer will do so.

The comination of non-blocking dequeue and automatic mute on starvation does
make startup more difficult than might otherwise be the case. During startup
decoded buffers will be rapidly consume as the ALSA playback buffers can be
filled without any blocking due to ALSAs playback speed. In this circumstance
underflow of mixer input is inevitable since no underflow 
would indicate that there is too much buffering seperating the decoder and the
mixer.

There are a number of solutions to the problem the simplest being to fill the
ALSA playback buffers with silence before starting up for real. This results in
a startup latency that is as long as the ALSA playback buffer. For a triple
buffered output at 48KHz (feeding an AC3 encoder with a frame size of 1536)
this results in a delay of approximately 96ms.

\section interactive_mgt_sec Interactive buffer management

TODO

\section output_mgt_sec Output buffer management

Profile 1 requires four outputs to be run simulatenously however these are controlled in agregate 

\section sychronization_sec Synchronization

STx710x series devices have two PCM players and one SPDIF player. Each has
its own independant frequency sythesiser (FSynth). While programming each one
to a constant frequency is possible it is not possible to tune them
simultaneously. Thus on any platform with a similar architecture each of
the outputs must be subject to independant synchronization.

STi7200 devices have four PCM players connected directly via I2S to DACs
(internal and external), they have an SPDIF player serving true SPDIF output,
and finally they have a pair of players (one PCM player and one SPDIF player
the use of which is mutually exclusive) serving HDMI audio. The four DAC based
PCM players are all served by a single frequency synthesiser and therefore will
not drift relative to one another. The SPDIF and HDMI audio outputs are served
by another frequency synthesiser which must be kept in sync with the first via
a secondary process. The inflexibility of the frequency synthesisers does imply
that if second room audio if connected to a real-time source then the only means
of clock recovery available to us is sample rate conversion. 

\subsection fsynth_equations_subsec Frequency Synthesizer Equations

The fundamental Fsynth equations is as follows:

<pre>
                             32768*Fpll
Fout = ------------------------------------------------------
                       md                        (md + 1)
        sdiv*((pe*(1 + --)) - ((pe - 32768)*(1 + --------)))
                       32                           32
</pre>

Where:

 - Fpll and Fout are frequencies in Hz
 - pdiv is power of 2 between 1 and 8
 - md is an integer between -1 and -16
 - pe is an integer between 0 and 32767

This has the following simplification:

<pre>
                  1048576*Fpll
Fout = ----------------------------------
        sdiv*(1081344 - pe + (32768*md))
</pre>

\section audio_todo_sec Development Plan

 - [COMPLETE] Stereo playback via internal DAC (STx7109).
 - [COMPLETE] Stereo playback with mixer firmware transfering samples (STx7109).
 - [https://bugzilla.stlinux.com/show_bug.cgi?id=1760] Migrate downmix from decoder to mixer (STx7109).
 - Dynamic switching of output mode (sampling frequency) based on stream parameters (STx7109).
 - [https://bugzilla.stlinux.com/show_bug.cgi?id=1759] Silence stuffing when input is not present (STx7109).
    - 'Idle' with 0 connected inputs but software muted output buffers.
    - Inject silence on input underflow.
    - Apply fade in, fade out to stream discontinuities due to underflow.
    - Apply fade in, fade out on demarked stream discontinuities (e.g. track change, trick mode).
 - Clock recovery and A/V sync (STx7109)
    - Precisely timed startup.
    - Continuous adjustment of Fsynth to prevent drift.
    - Mute based approach to significant discrepancies.
 - Primary and secondary decode and mix (STx7109).
 - ?Synchronization of secondary decode (via SRC)?.
 - Primary and interactive decode and mix (STx7109).
 - Multiple outputs (STx7109).
    - Manage output buffers to external DAC/HDMI and SPDIF.
    - Arrange appropriate clock recovery and startup 
 - Port to production device (STm7200).
    - Implement ALSA devices for STm7200.
    - Adapt the clock recovery and A/V sync management for STm7200.
 - Audio stable notification: https://bugzilla.stlinux.com/show_bug.cgi?id=1405
 
Last updated: 11-Jun-2007
 
*/



