/******************************************************************************

    AudioScience HPI driver
    Copyright (C) 1997-2003  AudioScience Inc. <support@audioscience.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of version 2 of the GNU General Public License as
    published by the Free Software Foundation;

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 Hardware Programming Interface (HPI) header
 The HPI is a low-level hardware abstraction layer to all
 AudioScience digital audio adapters

 You must define one operating systems that the HPI is to be compiled under
 HPI_OS_WIN16		  16bit Windows
 HPI_OS_WIN32_USER	  32bit Windows
 HPI_OS_DSP_563XX	  DSP563XX environment
 HPI_OS_DSP_C6000	  DSP TI C6000 environment
 HPI_OS_WDM			  Windows WDM kernel driver
 HPI_OS_LINUX		  Linux kernel driver

(C) Copyright AudioScience Inc. 1998-2007
******************************************************************************/
#ifndef _HPI_H_
#define _HPI_H_

/* HPI Version
If HPI_VER_MINOR is odd then its a development release not intended for the
public. If HPI_VER_MINOR is even then is a release version
i.e 3.05.02 is a development version
*/
#define HPI_VERSION_CONSTRUCTOR(maj, min, rel) \
	((maj << 16) + (min << 8) + rel)

#define HPI_VER_MAJOR(v) (v >> 16)
#define HPI_VER_MINOR(v) ((v >> 8) & 0xFF)
#define HPI_VER_RELEASE(v) (v & 0xFF)

/* Use single digits for versions less that 10 to avoid octal. */
#define HPI_VER HPI_VERSION_CONSTRUCTOR(3L, 9, 9)

#ifdef _DOX_ONLY_
/*****************************************************************************/
/** \defgroup compile_time_defines HPI compile time defines

This section descibes the usage of HPI defines to set the target compile
environment.
The definitions are used in the build environment to control how HPI is built.
They are NOT predefined in hpi.h!
\{
*/
/** \defgroup os_type_defines HPI_OS_XXX build environment defines

Define exactly one of these depending on which OS you are compiling for.
Should we also include DSP_53XXX, C6000, WDM etc?

\{
*/
/**Define when creating a 16 bit Windows user application. */
#define HPI_OS_WIN16
/** Define when creating a 32 bit Windows user application. */
#define HPI_OS_WIN32_USER
/** Define when creating a Linux application.*/
#define HPI_OS_LINUX
/* Should we also include DSP_53XXX, C6000, WDM etc? */
/**\}*/
/** \def HPI64BIT
Define this when building a 64 bit application.
When not defined a 32 bit environment will be assumed.
Currently only supported under Linux (autodetected).
*/
#define HPI64BIT

/** Define this to remove public definition of deprecated functions and defines.
Use this to reveal where the deprecated functionality is used
*/
#define HPI_EXCLUDE_DEPRECATED
/** \defgroup hpi_dll_defines HPI DLL function attributes
DLL environment defines.
\{
*/
/** Define when building an application that links to ASIHPI32.LIB
   (imports HPI functions from ASIHPI32.DLL). */
#define HPIDLL_IMPORTS
/** Define when creating an application that uses the HPI DLL. */
#define HPIDLL_EXPORTS
/** Define when building an application that compiles in
    HPIFUNC.C and does not use ASIHPI32.DLL */
#define HPIDLL_STATIC
/**\}*/

/** \defgroup hpi_dspcode_defines HPI DSP code loading method
Define exactly one of these to select how the DSP code is supplied to
the adapter.

End users writing applications that use the HPI interface do not have to
use any of the below defines.
They are only necessary for building drivers and 16 bit Windows applications.
\{
*/
/** DSP code is supplied as a series of arrays that are linked into
    the driver/application. Only valid when \ref HPI_OS_WIN16 is defined. */
#define HPI_DSPCODE_ARRAY
/** DSP code is supplied as a file that is opened and read from by the driver.*/
#define HPI_DSPCODE_FILE
/**  DSP code is read using the hotplug firmware loader module.
     Only valid when compiling the HPI kernel driver under Linux. */
#define HPI_DSPCODE_FIRMWARE
/**\}*/
/**\}*/
/*****************************************************************************/
#endif				/* #ifdef _DOX_ONLY_ */

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

/** maximum number of memory regions mapped to an adapter */
#define HPI_MAX_ADAPTER_MEM_SPACES (2)

#include "hpios.h"

/* A few convenience macros */
#ifndef DEPRECATED
#define DEPRECATED
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(A) (sizeof(A)/sizeof((A)[0]))
#endif

#ifndef __stringify
#define __stringify_1(x) #x
#define __stringify(x) __stringify_1(x)
#endif

#define HPI_UNUSED(param) (void)param

/******************************************************************************/
/******************************************************************************/
/********	HPI API DEFINITIONS					  *****/
/******************************************************************************/
/******************************************************************************/

/* //////////////////////////////////////////////////////////////////////// */
/* BASIC TYPES */
/* u8, u16, u32 MUST BE DEFINED IN HPIOS_xxx.H */

/* ////////////////////////////////////////////////////////////////////// */
/** \addtogroup hpi_defines HPI constant definitions
\{
*/

/*******************************************/
/** \defgroup adapter_ids Adapter types/product ids
\{
*/
/** TI's C6701 EVM has this ID */
#define HPI_ADAPTER_EVM6701		0x1002
/** DSP56301 rev A has this ID */
#define HPI_ADAPTER_DSP56301		0x1801
 /** TI's PCI2040 PCI I/F chip has this ID */
#define HPI_ADAPTER_PCI2040		0xAC60
/** TI's C6205 PCI interface has this ID */
#define HPI_ADAPTER_DSP6205		0xA106
/** First 2 hex digits define the adapter family */
#define HPI_ADAPTER_FAMILY_MASK		0xff00

/* OBSOLETE - ASI1101 - 1 Stream MPEG playback */
/* #define HPI_ADAPTER_ASI1101		0x1101	*/
/*OBSOLETE - ASI1201 - 2 Stream MPEG playback */
/* #define HPI_ADAPTER_ASI1201		0x1201	*/

#define HPI_ADAPTER_FAMILY_ASI1700	0x1700
/** ASI1711 - Quad FM+RDS tuner module */
#define HPI_ADAPTER_ASI1711		0x1711
/** ASI1721 - Quad AM/FM+RDS tuner module */
#define HPI_ADAPTER_ASI1721		0x1721
/** ASI1731 - Quad TV tuner module */
#define HPI_ADAPTER_ASI1731		0x1731
/*ASI2214 - USB 2.0 1xanalog in, 4 x analog out, 1 x AES in/out */
/*#define HPI_ADAPTER_ASI2214		0x2214	*/

/** ASI2416 - CobraNet peripheral */
#define HPI_ADAPTER_ASI2416		0x2416

/*ASI4030 = PSI30 = OEM 3 Stereo playback */
/*#define HPI_ADAPTER_ASI4030		0x4030*/

#define HPI_ADAPTER_FAMILY_ASI4100	0x4100
/** 2 play-1out, 1 rec PCM, MPEG*/
#define HPI_ADAPTER_ASI4111		0x4111
/** 4 play-3out, 1 rec PCM, MPEG*/
#define HPI_ADAPTER_ASI4113		0x4113
/** 4 play-5out, 1 rec, PCM, MPEG*/
#define HPI_ADAPTER_ASI4215		0x4215

#define HPI_ADAPTER_FAMILY_ASI4300	0x4300
/** 4 play-1out, 1 rec-1in, PCM, MPEG*/
#define HPI_ADAPTER_ASI4311		0x4311
/** 4 play-2out, 1 rec-1in, PCM, MPEG*/
#define HPI_ADAPTER_ASI4312		0x4312
/** 4 play-2out, 1 rec-3in, PCM, MPEG*/
#define HPI_ADAPTER_ASI4332		0x4332
/** 4 play-4out, 1 rec-3in, PCM, MPEG*/
#define HPI_ADAPTER_ASI4334		0x4334
/** 4 play-4out, 1 rec-3in, PCM, MPEG, 8-relay, 16-opto*/
#define HPI_ADAPTER_ASI4335		0x4335
/** 4 play-4out, 1 rec-3in, PCM, MPEG, 8-relay, 16-opto, RS422*/
#define HPI_ADAPTER_ASI4336		0x4336
/** (ASI4312 with MP3) 4 play-2out, 1 rec-1in, PCM, MPEG-L2, MP3 */
#define HPI_ADAPTER_ASI4342		0x4342
/** (ASI4334 with MP3)4 play-4out, 1 rec-3in, PCM, MPEG-L2, MP3 */
#define HPI_ADAPTER_ASI4344		0x4344
/** (ASI4336 with MP3)4 play-4out, 1 rec-3in, PCM,
	MPEG-L2, MP3, 8-relay, 16-opto, RS422*/
#define HPI_ADAPTER_ASI4346		0x4346

/* OEM 2 play, PCM mono/stereo, 44.1kHz*/
/*#define HPI_ADAPTER_ASI4401		0x4401 */
/*#define HPI_ADAPTER_ASI4501		0x4501
OBSOLETE - OEM 4 play, 1 rec PCM, MPEG*/
/*#define HPI_ADAPTER_ASI4502		0x4502
	OBSOLETE - OEM 1 play, 1 rec PCM, MPEG*/
/*#define HPI_ADAPTER_ASI4503		0x4503
OBSOLETE - OEM 4 play PCM, MPEG*/
/*#define HPI_ADAPTER_ASI4601		0x4601
	OEM 4 play PCM, MPEG & 1 record with AES-18 */
/*#define HPI_ADAPTER_ASI4701		0x4701
	OEM 24 mono play PCM with 512MB RAM */

#define HPI_ADAPTER_FAMILY_ASI5000	0x5000
/** ASI5001 OEM, PCM only, 4 in, 1 out analog */
#define HPI_ADAPTER_ASI5001		0x5001
/** ASI5002 OEM, PCM only, 4 in, 1 out analog and digital */
#define HPI_ADAPTER_ASI5002		0x5002
/** ASI5020 PCM only, 2 analog only in/out */
#define HPI_ADAPTER_ASI5020		0x5020
/** ASI5044 PCM only, 4 analog and digital in/out */
#define HPI_ADAPTER_ASI5044		0x5044
/** ASI5041 PCM only, 4 digital only in/out */
#define HPI_ADAPTER_ASI5041		0x5041
/** ASI5042 PCM only, 4 analog only in/out */
#define HPI_ADAPTER_ASI5042		0x5042

#define HPI_ADAPTER_FAMILY_ASI5100	0x5100
/** ASI5101 OEM is ASI5111 with no mic. */
#define HPI_ADAPTER_ASI5101		0x5101
/** ASI5111 PCM only */
#define HPI_ADAPTER_ASI5111		0x5111

/** ASI6101 prototype */
#define HPI_ADAPTER_ASI6101		0x6101

#define HPI_ADAPTER_FAMILY_ASI6000	0x6000
/** ASI6000 - generic 1 DSP adapter, exact config undefined */
#define HPI_ADAPTER_ASI6000		0x6000
/** ASI6012 - 1 in, 2 out analog only */
#define HPI_ADAPTER_ASI6012		0x6012
/** ASI6022 - 2 in, 2 out analog only */
#define HPI_ADAPTER_ASI6022		0x6022
/** ASI6044 - 4 in/out analog only */
#define HPI_ADAPTER_ASI6044		0x6044

#define HPI_ADAPTER_FAMILY_ASI6100	0x6100
/** ASI6111 - 1 in/out, analog and AES3  */
#define HPI_ADAPTER_ASI6111		0x6111
/** ASI6102 - 2out,analog and AES3  */
#define HPI_ADAPTER_ASI6102		0x6102
/** 300MHz version of ASI6114 for testing*/
#define HPI_ADAPTER_ASI6113		0x6113
/** ASI6122 - 2 in/out, analog and AES3  */
#define HPI_ADAPTER_ASI6122		0x6122
/** ASI6114 - 4os,1is,4out,1in,analog and AES3	*/
#define HPI_ADAPTER_ASI6114		0x6114
/** ASI6118 - 8os,1is,8out,1in analog+AES3 */
#define HPI_ADAPTER_ASI6118		0x6118

#define HPI_ADAPTER_FAMILY_ASI6200	0x6200
/** ASI6201 - OEM	*/
#define HPI_ADAPTER_ASI6201		0x6201
/** ASI6244 - 4os,4is,4out,4in,analog and AES3 */
#define HPI_ADAPTER_ASI6244		0x6244
/** ASI6246 - 6os,2is,6out,4in,analog and AES3 */
#define HPI_ADAPTER_ASI6246		0x6246
/** ASI6200 - generic 2 DSP adapter, exact config undefined */
#define HPI_ADAPTER_ASI6200		0x6200
/** ASI6100 - generic 1 DSP adapter, exact config undefined */
#define HPI_ADAPTER_ASI6100		0x6100

/* Represents 6205 DSP code for HPI6205 based adapters */
#define HPI_ADAPTER_FAMILY_ASI6205	0x6205

#define HPI_ADAPTER_FAMILY_ASI6400	0x6400
/** ASI6408 - cobranet PCI 8 mono in/out */
#define HPI_ADAPTER_ASI6408		0x6408
/** ASI6416 - cobranet PCI 16 mono in/out */
#define HPI_ADAPTER_ASI6416		0x6416

/** ASI6500 PCI sound cards */
#define HPI_ADAPTER_FAMILY_ASI6500	0x6500
/** ASI6511 - 1 in/out, analog and AES3  */
#define HPI_ADAPTER_ASI6511		0x6511
/** ASI6514 - ASI6114 replacement, 12os,2is,4out,1in,analog and AES3  */
#define HPI_ADAPTER_ASI6514		0x6514
/** ASI6518 - ASI6118 replacement, 8os,1is,8out,1in analog+AES3 */
#define HPI_ADAPTER_ASI6518		0x6518
/** ASI6520 - 6os,4is,2out,2in,analog only  */
#define HPI_ADAPTER_ASI6520		0x6520
/** ASI6522 - 6os,4is,2out,2in,analog and AES3	*/
#define HPI_ADAPTER_ASI6522		0x6522
/** ASI6540 - 12os,8is,4out,4in,analog only  */
#define HPI_ADAPTER_ASI6540		0x6540
/** ASI6544 - 12os,8is,4out,4in,analog and AES3  */
#define HPI_ADAPTER_ASI6544		0x6544
/** ASI6548 - 16os,8is,8out,4in,analog and AES3  */
#define HPI_ADAPTER_ASI6548		0x6548
/** ASI6585  - 8in, 8out, Livewire */
#define HPI_ADAPTER_ASI6585		0x6585

/** ASI6600 PCI Express sound cards */
#define HPI_ADAPTER_FAMILY_ASI6600	0x6600
/** ASI6611 - 1 in/out, analog and AES3  */
#define HPI_ADAPTER_ASI6611		0x6611
/** ASI6614 - ASI6114 replacement, 12os,2is,4out,1in,analog and AES3  */
#define HPI_ADAPTER_ASI6614		0x6614
/** ASI6618 - ASI6118 replacement, 8os,1is,8out,1in analog+AES3 */
#define HPI_ADAPTER_ASI6618		0x6618
/** ASI6620 - 6os,4is,2out,2in,analog only  */
#define HPI_ADAPTER_ASI6620		0x6620
/** ASI6622 - 6os,4is,2out,2in,analog and AES3	*/
#define HPI_ADAPTER_ASI6622		0x6622
/** ASI6640 - 12os,8is,4out,4in,analog only  */
#define HPI_ADAPTER_ASI6640		0x6640
/** ASI6644 - 12os,8is,4out,4in,analog and AES3  */
#define HPI_ADAPTER_ASI6644		0x6644
/** ASI6648 - 16os,8is,8out,4in,analog and AES3  */
#define HPI_ADAPTER_ASI6648		0x6648
/** ASI6685  - 8in, 8out, Livewire */
#define HPI_ADAPTER_ASI6685		0x6685

/*#define HPI_ADAPTER_ASI8401		0x8401	OEM 4 record */
/*#define HPI_ADAPTER_ASI8411		0x8411	OEM RF switcher */
/*#define HPI_ADAPTER_ASI8601		0x8601	OEM 8 record */

#define HPI_ADAPTER_FAMILY_ASI8700	0x8700
/** OEM 8 record 2 AM/FM + 6 FM/TV , AM has 10kHz b/w*/
#define HPI_ADAPTER_ASI8701		0x8701
/** 8 AM/FM record */
#define HPI_ADAPTER_ASI8702		0x8702
/** 8 TV/FM record */
#define HPI_ADAPTER_ASI8703		0x8703
/** standard product 2 AM/FM + 6 FM/TV */
#define HPI_ADAPTER_ASI8704		0x8704
/** 4 TV/FM, 4 AM/FM record */
#define HPI_ADAPTER_ASI8705		0x8705
/** 8 record 2 AM/FM + 6 FM/TV + 2 ext antenna jacks*/
#define HPI_ADAPTER_ASI8706		0x8706
/** 8 record AM/FM - 4 ext antenna jacks */
#define HPI_ADAPTER_ASI8707		0x8707
/** 8 record AM/FM - 6 ext antenna jacks */
#define HPI_ADAPTER_ASI8708		0x8708
/** 8 record - no tuners */
#define HPI_ADAPTER_ASI8709		0x8709
/** 8 record AM/FM - 1 ext antenna jacks*/
#define HPI_ADAPTER_ASI8710		0x8710
/** 8 record AM/FM - 2 ext antenna jacks*/
#define HPI_ADAPTER_ASI8711		0x8711

/** 4 record AM/FM */
#define HPI_ADAPTER_ASI8712		0x8712
/** 4 record NTSC-TV/FM */
#define HPI_ADAPTER_ASI8713		0x8713

	/** 8 record 6xAM/FM+2xNTSC */
#define HPI_ADAPTER_ASI8722		0x8722
/** 8 record NTSC */
#define HPI_ADAPTER_ASI8723		0x8723
/** 4 record NTSC */
#define HPI_ADAPTER_ASI8724		0x8724
/** 4 record 4xAM/FM+4xNTSC */
#define HPI_ADAPTER_ASI8725		0x8725

	/** 8 record 6xAM/FM+2xPAL */
#define HPI_ADAPTER_ASI8732		0x8732
/** 8 record PAL */
#define HPI_ADAPTER_ASI8733		0x8733
/** 4 record PAL */
#define HPI_ADAPTER_ASI8734		0x8734
/** 4 record 4xAM/FM+4xPAL */
#define HPI_ADAPTER_ASI8735		0x8735

#define HPI_ADAPTER_FAMILY_ASI8800	0x8800
/** OEM 8 record */
#define HPI_ADAPTER_ASI8801		0x8801

#define HPI_ADAPTER_FAMILY_ASI8900	0x8900
/** 2 module tuner card */
#define HPI_ADAPTER_ASI8920		0x8920

/** Used in DLL to indicate device not present */
#define HPI_ADAPTER_ILLEGAL		0xFFFF
/**\}*/

/*******************************************/
/** \defgroup formats Audio format types
\{
*/
/** Used internally on adapter. */
#define HPI_FORMAT_MIXER_NATIVE		0
/** 8-bit unsigned PCM. Windows equivalent is WAVE_FORMAT_PCM. */
#define HPI_FORMAT_PCM8_UNSIGNED	1
/** 16-bit signed PCM. Windows equivalent is WAVE_FORMAT_PCM. */
#define HPI_FORMAT_PCM16_SIGNED		2
/** MPEG-1 Layer-1. */
#define HPI_FORMAT_MPEG_L1		3
/** MPEG-1 Layer-2.

Windows equivalent is WAVE_FORMAT_MPEG.

The following table shows what combinations of mode and bitrate are possible:

<table border=1 cellspacing=0 cellpadding=5>
<tr>
<td><p><b>Bitrate (kbs)</b></p>
<td><p><b>Mono</b></p>
<td><p><b>Stereo,<br>Joint Stereo or<br>Dual Channel</b></p>

<tr><td>32<td>X<td>_
<tr><td>40<td>_<td>_
<tr><td>48<td>X<td>_
<tr><td>56<td>X<td>_
<tr><td>64<td>X<td>X
<tr><td>80<td>X<td>_
<tr><td>96<td>X<td>X
<tr><td>112<td>X<td>X
<tr><td>128<td>X<td>X
<tr><td>160<td>X<td>X
<tr><td>192<td>X<td>X
<tr><td>224<td>_<td>X
<tr><td>256<td>-<td>X
<tr><td>320<td>-<td>X
<tr><td>384<td>_<td>X
</table>
*/
#define HPI_FORMAT_MPEG_L2				4
/** MPEG-1 Layer-3.
Windows equivalent is WAVE_FORMAT_MPEG.

The following table shows what combinations of mode and bitrate are possible:

<table border=1 cellspacing=0 cellpadding=5>
<tr>
<td><p><b>Bitrate (kbs)</b></p>
<td><p><b>Mono<br>Stereo @ 8,<br>11.025 and<br>12kHz*</b></p>
<td><p><b>Mono<br>Stereo @ 16,<br>22.050 and<br>24kHz*</b></p>
<td><p><b>Mono<br>Stereo @ 32,<br>44.1 and<br>48kHz</b></p>

<tr><td>8<td>X<td>X<td>_
<tr><td>16<td>X<td>X<td>_
<tr><td>24<td>X<td>X<td>_
<tr><td>32<td>X<td>X<td>X
<tr><td>40<td>X<td>X<td>X
<tr><td>48<td>X<td>X<td>X
<tr><td>56<td>X<td>X<td>X
<tr><td>64<td>X<td>X<td>X
<tr><td>80<td>_<td>X<td>X
<tr><td>96<td>_<td>X<td>X
<tr><td>112<td>_<td>X<td>X
<tr><td>128<td>_<td>X<td>X
<tr><td>144<td>_<td>X<td>_
<tr><td>160<td>_<td>X<td>X
<tr><td>192<td>_<td>_<td>X
<tr><td>224<td>_<td>_<td>X
<tr><td>256<td>-<td>_<td>X
<tr><td>320<td>-<td>_<td>X
</table>
\b * Available on the ASI6000 series only
*/
#define HPI_FORMAT_MPEG_L3		5
/** Dolby AC-2. */
#define HPI_FORMAT_DOLBY_AC2		6
/** Dolbt AC-3. */
#define HPI_FORMAT_DOLBY_AC3		7
/** 16-bit PCM big-endian. */
#define HPI_FORMAT_PCM16_BIGENDIAN	8
/** TAGIT-1 algorithm - hits. */
#define HPI_FORMAT_AA_TAGIT1_HITS	9
/** TAGIT-1 algorithm - inserts. */
#define HPI_FORMAT_AA_TAGIT1_INSERTS	10
/** 32-bit signed PCM. Windows equivalent is WAVE_FORMAT_PCM.
Each sample is a 32bit word. The most significant 24 bits contain a 24-bit
sample and the least significant 8 bits are set to 0.
*/
#define HPI_FORMAT_PCM32_SIGNED		11
/** Raw bitstream - unknown format. */
#define HPI_FORMAT_RAW_BITSTREAM	12
/** TAGIT-1 algorithm hits - extended. */
#define HPI_FORMAT_AA_TAGIT1_HITS_EX1	13
/** 32-bit PCM as an IEEE float. Windows equivalent is WAVE_FORMAT_IEEE_FLOAT.
Each sample is a 32bit word in IEEE754 floating point format.
The range is +1.0 to -1.0, which corresponds to digital fullscale.
*/
#define HPI_FORMAT_PCM32_FLOAT		14
/** 24-bit PCM signed. Windows equivalent is WAVE_FORMAT_PCM. */
#define HPI_FORMAT_PCM24_SIGNED		15
/** OEM format 1 - private. */
#define HPI_FORMAT_OEM1			16
/** OEM format 2 - private. */
#define HPI_FORMAT_OEM2			17
/** Undefined format. */
#define HPI_FORMAT_UNDEFINED		(0xffff)
/**\}*/

/******************************************* bus types */
#define HPI_BUS_ISAPNP		1
#define HPI_BUS_PCI		2
#define HPI_BUS_USB		3

/******************************************* in/out Stream states */
/*******************************************/
/** \defgroup stream_states Stream States
\{
*/
/** State stopped - stream is stopped. */
#define HPI_STATE_STOPPED	1
/** State playing - stream is playing audio. */
#define HPI_STATE_PLAYING	2
/** State recording - stream is recording. */
#define HPI_STATE_RECORDING	3
/** State drained - playing stream ran out of data to play. */
#define HPI_STATE_DRAINED	4
/** State generate sine - to be implemented. */
#define HPI_STATE_SINEGEN	5
/**\}*/
/******************************************* mixer source node types */
/** \defgroup source_nodes Source node types
\{
*/
#define HPI_SOURCENODE_BASE			100
/** Out Stream (Play) node. */
#define HPI_SOURCENODE_OSTREAM			101
#define HPI_SOURCENODE_LINEIN			102 /**< Line in node. */
#define HPI_SOURCENODE_AESEBU_IN		103 /**< AES/EBU input node. */
#define HPI_SOURCENODE_TUNER			104 /**< Tuner node. */
#define HPI_SOURCENODE_RF			105 /**< RF input node. */
#define HPI_SOURCENODE_CLOCK_SOURCE		106 /**< Clock source node. */
#define HPI_SOURCENODE_RAW_BITSTREAM		107 /**< Raw bitstream node. */
#define HPI_SOURCENODE_MICROPHONE		108 /**< Microphone node. */
/** Cobranet input node.
    Audio samples come from the Cobranet network and into the device. */
#define HPI_SOURCENODE_COBRANET			109
/*! Update this if you add a new sourcenode type, AND hpidebug.h */
#define HPI_SOURCENODE_LAST_INDEX		109
/* AX4 max sourcenode type = 15 */
/* AX6 max sourcenode type = 15 */
/**\}*/

/******************************************* mixer dest node types */
/** \defgroup dest_nodes Destination node types
\{
*/
#define HPI_DESTNODE_BASE			200
/** In Stream (Record) node. */
#define HPI_DESTNODE_ISTREAM			201
#define HPI_DESTNODE_LINEOUT			202 /**< Line Out node. */
#define HPI_DESTNODE_AESEBU_OUT			203 /**< AES/EBU output node. */
#define HPI_DESTNODE_RF				204 /**< RF output node. */
#define HPI_DESTNODE_SPEAKER			205 /**< Speaker output node. */
/** Cobranet output node.
Audio samples from the device are sent out on the Cobranet network.*/
#define HPI_DESTNODE_COBRANET			206
/*! Update this if you add a new destnode type. , AND hpidebug.h  */
#define HPI_DESTNODE_LAST_INDEX			206
/* AX4 max destnode type = 7 */
/* AX6 max destnode type = 15 */
/**\}*/

/*******************************************/
/** \defgroup control_types Mixer control types
\{
*/
#define HPI_CONTROL_GENERIC		0	/**< Generic control. */
#define HPI_CONTROL_CONNECTION		1 /**< A connection between nodes. */
#define HPI_CONTROL_VOLUME		2 /**< Volume control - works in dBFs. */
#define HPI_CONTROL_METER		3	/**< Peak meter control. */
#define HPI_CONTROL_MUTE		4	/*Mute control - not used at present. */
#define HPI_CONTROL_MULTIPLEXER		5	/**< Multiplexer control. */
#define HPI_CONTROL_AESEBU_TRANSMITTER	6	/**< AES/EBU transmitter control. */
#define HPI_CONTROL_AESEBU_RECEIVER	7	/**< AES/EBU receiver control. */
#define HPI_CONTROL_LEVEL		8	/**< Level/trim control - works in dBu. */
#define HPI_CONTROL_TUNER		9	/**< Tuner control. */
/*#define HPI_CONTROL_ONOFFSWITCH	10 */
#define HPI_CONTROL_VOX			11	/**< Vox control. */
#define HPI_CONTROL_AES18_TRANSMITTER	12	/**< AES-18 transmitter control. */
#define HPI_CONTROL_AES18_RECEIVER	13	/**< AES-18 receiver control. */
#define HPI_CONTROL_AES18_BLOCKGENERATOR 14	/**< AES-18 block generator control. */
#define HPI_CONTROL_CHANNEL_MODE	15	/**< Channel mode control. */

/* WARNING types 16 or greater impact bit packing in
   AX4100 and AX4500 DSP code */
#define HPI_CONTROL_BITSTREAM		16	/**< Bitstream control. */
#define HPI_CONTROL_SAMPLECLOCK		17	/**< Sample clock control. */
#define HPI_CONTROL_MICROPHONE		18	/**< Microphone control. */
#define HPI_CONTROL_PARAMETRIC_EQ	19	/**< Parametric EQ control. */
#define HPI_CONTROL_COMPANDER		20	/**< Compander control. */
#define HPI_CONTROL_COBRANET		21	/**< Cobranet control. */
#define HPI_CONTROL_TONEDETECTOR	22	/**< Tone detector control. */
#define HPI_CONTROL_SILENCEDETECTOR	23	/**< Silence detector control. */

/*! Update this if you add a new control type. , AND hpidebug.h */
#define HPI_CONTROL_LAST_INDEX			23

/* WARNING types 32 or greater impact bit packing in all AX4 DSP code */
/* WARNING types 256 or greater impact bit packing in all AX6 DSP code */
/**\}*/

/* Shorthand names that match attribute names */
#define HPI_CONTROL_AESEBUTX	HPI_CONTROL_AESEBU_TRANSMITTER
#define HPI_CONTROL_AESEBURX	HPI_CONTROL_AESEBU_RECEIVER
#define HPI_CONTROL_EQUALIZER	HPI_CONTROL_PARAMETRIC_EQ

/******************************************* ADAPTER ATTRIBUTES ****/

/** \defgroup adapter_properties Adapter properties
These are used in HPI_AdapterSetProperty() and HPI_AdapterGetProperty()
\{
*/
/** \internal Used in dwProperty field of HPI_AdapterSetProperty() and
HPI_AdapterGetProperty(). This errata applies to all ASI6000 cards with both
analog and digital outputs. The CS4224 A/D+D/A has a one sample delay between
left and right channels on both its input (ADC) and output (DAC).
More details are available in Cirrus Logic errata ER284B2.
PDF available from www.cirrus.com, released by Cirrus in 2001.
*/
#define HPI_ADAPTER_PROPERTY_ERRATA_1		1

/** Adapter grouping property
Indicates whether the adapter supports the grouping API (for ASIO and SSX2)
*/
#define HPI_ADAPTER_PROPERTY_GROUPING		2

/** Adapter SSX2 property
Indicates whether the adapter supports SSX2 multichannel streams
*/
#define HPI_ADAPTER_PROPERTY_ENABLE_SSX2	3

/** Base number for readonly properties */
#define HPI_ADAPTER_PROPERTY_READONLYBASE	256

/** Readonly adapter latency property.
This property returns in the input and output latency in samples.
Property 1 is the estimated input latency
in samples, while Property 2 is that output latency in	samples.
*/
#define HPI_ADAPTER_PROPERTY_LATENCY (HPI_ADAPTER_PROPERTY_READONLYBASE+0)

/** Readonly adapter granularity property.
The granulariy is the smallest size chunk of stereo samples that is processed by
the adapter.
This property returns the record granularity in samples in Property 1.
Property 2 returns the play granularity.
*/
#define HPI_ADAPTER_PROPERTY_GRANULARITY (HPI_ADAPTER_PROPERTY_READONLYBASE+1)

/** Readonly adapter number of current channels property.
Property 1 is the number of record channels per record device.
Property 2 is the number of play channels per playback device.*/
#define HPI_ADAPTER_PROPERTY_CURCHANNELS (HPI_ADAPTER_PROPERTY_READONLYBASE+2)

/** Readonly adapter software version.
The SOFTWARE_VERSION property returns the version of the software running
on the adapter as Major.Minor.Release.
Property 1 contains Major in bits 15..8 and Minor in bits 7..0.
Property 2 contains Release in bits 7..0. */
#define HPI_ADAPTER_PROPERTY_SOFTWARE_VERSION \
(HPI_ADAPTER_PROPERTY_READONLYBASE+3)

/** Readonly adapter MAC address MSBs.
The MAC_ADDRESS_MSB property returns the most significant 32 bits of the MAC address.
Property 1 contains bits 47..32 of the MAC address.
Property 2 contains bits 31..16 of the MAC address. */
#define HPI_ADAPTER_PROPERTY_MAC_ADDRESS_MSB \
(HPI_ADAPTER_PROPERTY_READONLYBASE+4)

/** Readonly adapter MAC address LSBs
The MAC_ADDRESS_LSB property returns the least significant 16 bits of the MAC address.
Property 1 contains bits 15..0 of the MAC address. */
#define HPI_ADAPTER_PROPERTY_MAC_ADDRESS_LSB \
(HPI_ADAPTER_PROPERTY_READONLYBASE+5)
/**\}*/

/** Used in wQueryOrSet field of HPI_AdapterSetModeEx(). */
#define HPI_ADAPTER_MODE_SET	(0)

/** Used in wQueryOrSet field of HPI_AdapterSetModeEx(). */
#define HPI_ADAPTER_MODE_QUERY (1)

/** \defgroup adapter_modes Adapter Modes
	These are used by HPI_AdapterSetModeEx()

\warning - more than 16 possible modes breaks
a bitmask in the Windows WAVE DLL
\{
*/
/** 4 outstream mode.
- ASI6114: 1 instream
- ASI6044: 4 instreams
- ASI6012: 1 instream
- ASI6102: no instreams
- ASI6022, ASI6122: 2 instreams
- ASI5111, ASI5101: 2 instreams
- ASI652x, ASI662x: 2 instreams
- ASI654x, ASI664x: 4 instreams
*/
#define HPI_ADAPTER_MODE_4OSTREAM (1)

/** 6 outstream mode.
- ASI6012: 1 instream,
- ASI6022, ASI6122: 2 instreams
- ASI652x, ASI662x: 4 instreams
*/
#define HPI_ADAPTER_MODE_6OSTREAM (2)

/** 8 outstream mode.
- ASI6114: 8 instreams
- ASI6118: 8 instreams
- ASI6585: 8 instreams
*/
#define HPI_ADAPTER_MODE_8OSTREAM (3)

/** 16 outstream mode.
- ASI6416 16 instreams
- ASI6518, ASI6618 16 instreams
- ASI6118 16 mono out and in streams
*/
#define HPI_ADAPTER_MODE_16OSTREAM (4)

/** one outstream mode.
- ASI5111 1 outstream, 1 instream
*/
#define HPI_ADAPTER_MODE_1OSTREAM (5)

/** ASI504X mode 1. 12 outstream, 4 instream 0 to 48kHz sample rates
	(see ASI504X datasheet for more info).
*/
#define HPI_ADAPTER_MODE_1 (6)

/** ASI504X mode 2. 4 outstreams, 4 instreams at 0 to 192kHz sample rates
	(see ASI504X datasheet for more info).
*/
#define HPI_ADAPTER_MODE_2 (7)

/** ASI504X mode 3. 4 outstreams, 4 instreams at 0 to 192kHz sample rates
	(see ASI504X datasheet for more info).
*/
#define HPI_ADAPTER_MODE_3 (8)

/** ASI504X multichannel mode.
	2 outstreams -> 4 line outs (1 to 8 channel streams),
	4 lineins -> 1 instream (1 to 8 channel streams) at 0-48kHz.
	For more info see the SSX Specification.
*/
#define HPI_ADAPTER_MODE_MULTICHANNEL (9)

/** 12 outstream mode.
- ASI6514, ASI6614: 2 instreams
- ASI6540,ASI6544: 8 instreams
- ASI6640,ASI6644: 8 instreams
*/
#define HPI_ADAPTER_MODE_12OSTREAM (10)

/** 9 outstream mode.
- ASI6044: 8 instreams
*/
#define HPI_ADAPTER_MODE_9OSTREAM (11)

/**\}*/

/* Note, adapters can have more than one capability -
encoding as bitfield is recommended. */
#define HPI_CAPABILITY_NONE		(0)
#define HPI_CAPABILITY_MPEG_LAYER3	(1)

/* Set this equal to maximum capability index,
Must not be greater than 32 - see axnvdef.h */
#define HPI_CAPABILITY_MAX			1
/* #define HPI_CAPABILITY_AAC		   2 */

/******************************************* STREAM ATTRIBUTES ****/

/* Ancillary Data modes */
#define HPI_MPEG_ANC_HASENERGY	(0)
#define HPI_MPEG_ANC_RAW	(1)
#define HPI_MPEG_ANC_ALIGN_LEFT (0)
#define HPI_MPEG_ANC_ALIGN_RIGHT (1)

/** \defgroup mpegmodes MPEG modes
\{
MPEG modes - can be used optionally for HPI_FormatCreate()
parameter dwAttributes.

The operation of the below modes varies according to the number of channels.
Using HPI_MPEG_MODE_DEFAULT causes the MPEG-1 Layer II bitstream to be recorded
in single_channel mode when the number of channels is 1 and in stereo when the
number of channels is 2. Using any mode setting other than HPI_MPEG_MODE_DEFAULT
when the number of channels is set to 1 will return an error.
*/
#define HPI_MPEG_MODE_DEFAULT		(0)
#define HPI_MPEG_MODE_STEREO		(1)
#define HPI_MPEG_MODE_JOINTSTEREO	(2)
#define HPI_MPEG_MODE_DUALCHANNEL	(3)
/** \} */

/* Locked memory buffer alloc/free phases */
/** use one message to allocate or free physical memory */
#define HPI_BUFFER_CMD_EXTERNAL			0
/** alloc physical memory */
#define HPI_BUFFER_CMD_INTERNAL_ALLOC		1
/** send physical memory address to adapter */
#define HPI_BUFFER_CMD_INTERNAL_GRANTADAPTER	2
/** notify adapter to stop using physical buffer */
#define HPI_BUFFER_CMD_INTERNAL_REVOKEADAPTER	3
/** free physical buffer */
#define HPI_BUFFER_CMD_INTERNAL_FREE		4

/******************************************* MIXER ATTRIBUTES ****/

/* \defgroup mixer_flags Mixer flags for HPI_MIXER_GET_CONTROL_MULTIPLE_VALUES
{
*/
#define HPI_MIXER_GET_CONTROL_MULTIPLE_CHANGED	(0)
#define HPI_MIXER_GET_CONTROL_MULTIPLE_RESET	(1)
/*}*/

/** Commands used by HPI_MixerStore()
*/
enum HPI_MIXER_STORE_COMMAND {
/** Save all mixer control settings. */
	HPI_MIXER_STORE_SAVE = 1,
/** Restore all controls from saved. */
	HPI_MIXER_STORE_RESTORE = 2,
/** Delete saved control settings. */
	HPI_MIXER_STORE_DELETE = 3,
/** Enable auto storage of some control settings. */
	HPI_MIXER_STORE_ENABLE = 4,
/** Disable auto storage of some control settings. */
	HPI_MIXER_STORE_DISABLE = 5,
/** Save the attributes of a single control. */
	HPI_MIXER_STORE_SAVE_SINGLE = 6
};

/******************************************* CONTROL ATTRIBUTES ****/
/* (in order of control type ID as above */

/* This allows for 255 control types, 256 unique attributes each */
#define HPI_CTL_ATTR(ctl, ai) (HPI_CONTROL_##ctl * 0x100 + ai)

/* Original 0-based non-unique attributes, might become unique later */
#define HPI_CTL_ATTR0(ctl, ai) (ai)

/* Generic control attributes.	If a control uses any of these attributes
   its other attributes must also be defined using HPI_CTL_ATTR()
*/

/** Enable a control.
0=disable, 1=enable
\note generic to all mixer plugins?
*/
#define HPI_GENERIC_ENABLE HPI_CTL_ATTR(GENERIC, 1)

/** Enable event generation for a control.
0=disable, 1=enable
\note generic to all controls that can generate events
*/
#define HPI_GENERIC_EVENT_ENABLE HPI_CTL_ATTR(GENERIC, 2)

/* Used by mixer plugin enable functions */
#define HPI_SWITCH_OFF		0	/**< Turn the mixer plugin on. */
#define HPI_SWITCH_ON		1	/**< Turn the mixer plugin off. */

/* Volume Control attributes */
#define HPI_VOLUME_GAIN			HPI_CTL_ATTR0(VOLUME, 1)
/** log fade - dB attenuation changes linearly over time */
#define HPI_VOLUME_AUTOFADE_LOG		HPI_CTL_ATTR0(VOLUME, 2)
#define HPI_VOLUME_AUTOFADE		HPI_VOLUME_AUTOFADE_LOG
/** linear fade - amplitude changes linearly */
#define HPI_VOLUME_AUTOFADE_LINEAR	HPI_CTL_ATTR0(VOLUME, 3)
/** Not implemented yet -may be special profiles */
#define HPI_VOLUME_AUTOFADE_1		HPI_CTL_ATTR0(VOLUME, 4)
#define HPI_VOLUME_AUTOFADE_2		HPI_CTL_ATTR0(VOLUME, 5)
/** For HPI_ControlQuery() to get the number of channels of a volume control*/
#define HPI_VOLUME_NUM_CHANNELS		HPI_CTL_ATTR0(VOLUME, 6)
#define HPI_VOLUME_RANGE		HPI_CTL_ATTR0(VOLUME, 10)

/** Level Control attributes */
#define HPI_LEVEL_GAIN			HPI_CTL_ATTR0(LEVEL, 1)
#define HPI_LEVEL_RANGE			HPI_CTL_ATTR0(LEVEL, 10)

/* Volume control special gain values */
/** volumes units are 100ths of a dB */
#define HPI_UNITS_PER_dB		100
/** turns volume control OFF or MUTE */
#define HPI_GAIN_OFF			(-100 * HPI_UNITS_PER_dB)
/** value returned for no signal */
#define HPI_METER_MINIMUM		(-150 * HPI_UNITS_PER_dB)

/* Meter Control attributes */
/** return RMS signal level */
#define HPI_METER_RMS			HPI_CTL_ATTR0(METER, 1)
/** return peak signal level */
#define HPI_METER_PEAK			HPI_CTL_ATTR0(METER, 2)
/** ballistics for ALL rms meters on adapter */
#define HPI_METER_RMS_BALLISTICS	HPI_CTL_ATTR0(METER, 3)
/** ballistics for ALL peak meters on adapter */
#define HPI_METER_PEAK_BALLISTICS	HPI_CTL_ATTR0(METER, 4)
/** For HPI_ControlQuery() to get the number of channels of a meter control*/
#define HPI_METER_NUM_CHANNELS		HPI_CTL_ATTR0(METER, 5)

/* Multiplexer control attributes */
#define HPI_MULTIPLEXER_SOURCE		HPI_CTL_ATTR0(MULTIPLEXER, 1)
#define HPI_MULTIPLEXER_QUERYSOURCE	HPI_CTL_ATTR0(MULTIPLEXER, 2)

/** AES/EBU transmitter control attributes */
/** AESEBU or SPDIF */
#define HPI_AESEBUTX_FORMAT		HPI_CTL_ATTR0(AESEBUTX, 1)
#define HPI_AESEBUTX_SAMPLERATE		HPI_CTL_ATTR0(AESEBUTX, 3)
#define HPI_AESEBUTX_CHANNELSTATUS	HPI_CTL_ATTR0(AESEBUTX, 4)
#define HPI_AESEBUTX_USERDATA		HPI_CTL_ATTR0(AESEBUTX, 5)

/** AES/EBU receiver control attributes */
#define HPI_AESEBURX_FORMAT		HPI_CTL_ATTR0(AESEBURX, 1)
#define HPI_AESEBURX_ERRORSTATUS	HPI_CTL_ATTR0(AESEBURX, 2)
#define HPI_AESEBURX_SAMPLERATE		HPI_CTL_ATTR0(AESEBURX, 3)
#define HPI_AESEBURX_CHANNELSTATUS	HPI_CTL_ATTR0(AESEBURX, 4)
#define HPI_AESEBURX_USERDATA		HPI_CTL_ATTR0(AESEBURX, 5)

/* NOTE: old defs only make sense when rx & tx not unique */
#define HPI_AESEBU_FORMAT		1
#define HPI_AESEBU_ERRORSTATUS		2
#define HPI_AESEBU_SAMPLERATE		3
#define HPI_AESEBU_CHANNELSTATUS	4
#define HPI_AESEBU_USERDATA		5

/** AES/EBU physical format - AES/EBU balanced "professional"  */
#define HPI_AESEBU_FORMAT_AESEBU	1
/** AES/EBU physical format - S/PDIF unbalanced "consumer"	*/
#define HPI_AESEBU_FORMAT_SPDIF		2

/** \defgroup aesebu_errors AES/EBU error status bits
\{ */

/**  bit0: 1 when PLL is not locked */
#define HPI_AESEBU_ERROR_NOT_LOCKED		0x01
/**  bit1: 1 when signal quality is poor */
#define HPI_AESEBU_ERROR_POOR_QUALITY		0x02
/** bit2: 1 when there is a parity error */
#define HPI_AESEBU_ERROR_PARITY_ERROR		0x04
/**  bit3: 1 when there is a bi-phase coding violation */
#define HPI_AESEBU_ERROR_BIPHASE_VIOLATION	0x08
/**  bit4: 1 when the validity bit is high */
#define HPI_AESEBU_ERROR_VALIDITY		0x10
/**\}*/

/** \defgroup tuner_defs Tuners
\{
*/
/** \defgroup tuner_attrs Tuner control attributes
\{
*/
#define HPI_TUNER_BAND			HPI_CTL_ATTR0(TUNER, 1)
#define HPI_TUNER_FREQ			HPI_CTL_ATTR0(TUNER, 2)
#define HPI_TUNER_LEVEL			HPI_CTL_ATTR0(TUNER, 3)
#define HPI_TUNER_AUDIOMUTE		HPI_CTL_ATTR0(TUNER, 4)
/* use TUNER_STATUS instead */
#define HPI_TUNER_VIDEO_STATUS		HPI_CTL_ATTR0(TUNER, 5)
#define HPI_TUNER_GAIN			HPI_CTL_ATTR0(TUNER, 6)
#define HPI_TUNER_STATUS		HPI_CTL_ATTR0(TUNER, 7)
#define HPI_TUNER_MODE			HPI_CTL_ATTR0(TUNER, 8)
/** RDS data. */
#define HPI_TUNER_RDS			HPI_CTL_ATTR0(TUNER, 9)
/** \} */

/** \defgroup tuner_bands Tuner bands

Used for HPI_Tuner_SetBand(),HPI_Tuner_GetBand()
\{
*/
#define HPI_TUNER_BAND_AM		1	 /**< AM band */
#define HPI_TUNER_BAND_FM		2	 /**< FM band (mono) */
#define HPI_TUNER_BAND_TV_NTSC_M	3	 /**< NTSC-M TV band*/
#define HPI_TUNER_BAND_TV		3	/* use TV_NTSC_M */
#define HPI_TUNER_BAND_FM_STEREO	4	 /**< FM band (stereo) */
#define HPI_TUNER_BAND_AUX		5	 /**< Auxiliary input */
#define HPI_TUNER_BAND_TV_PAL_BG	6	 /**< PAL-B/G TV band*/
#define HPI_TUNER_BAND_TV_PAL_I		7	 /**< PAL-I TV band*/
#define HPI_TUNER_BAND_TV_PAL_DK	8	 /**< PAL-D/K TV band*/
#define HPI_TUNER_BAND_TV_SECAM_L	9	 /**< SECAM-L TV band*/
#define HPI_TUNER_BAND_LAST 9 /**< The index of the last tuner band. */
/** \} */

/* Tuner mode attributes */
#define HPI_TUNER_MODE_RSS		1	/**< Tuner mode attribute RSS */

/* RSS attribute values */
#define HPI_TUNER_MODE_RSS_DISABLE 0 /**< Tuner mode attribute RSS disable */
#define HPI_TUNER_MODE_RSS_ENABLE  1 /**< Tuner mode attribute RSS enable */

/** Tuner Level settings */
#define HPI_TUNER_LEVEL_AVERAGE		0
#define HPI_TUNER_LEVEL_RAW		1

/** Tuner video status */
#define HPI_TUNER_VIDEO_COLOR_PRESENT		0x0001	/**< Video color is present. */
#define HPI_TUNER_VIDEO_IS_60HZ			0x0020	/**< 60 Hz video detected. */
#define HPI_TUNER_VIDEO_HORZ_SYNC_MISSING	0x0040	/**< Video HSYNC is missing. */
#define HPI_TUNER_VIDEO_STATUS_VALID		0x0100	/**< Video status is valid. */
#define HPI_TUNER_PLL_LOCKED			0x1000	/**< The tuner's PLL is locked. */
#define HPI_TUNER_FM_STEREO			0x2000	/**< Tuner reports back FM stereo. */
/** \} */

/* VOX control attributes */
#define HPI_VOX_THRESHOLD		HPI_CTL_ATTR0(VOX, 1)

/*?? channel mode used hpi_multiplexer_source attribute == 1 */
#define HPI_CHANNEL_MODE_MODE HPI_CTL_ATTR0(CHANNEL_MODE, 1)

/** \defgroup channel_modes Channel Modes
Used for HPI_ChannelModeSet/Get()
\{
*/
/** Left channel out = left channel in, Right channel out = right channel in. */
#define HPI_CHANNEL_MODE_NORMAL			1
/** Left channel out = right channel in, Right channel out = left channel in. */
#define HPI_CHANNEL_MODE_SWAP			2
/** Left channel out = left channel in, Right channel out = left channel in. */
#define HPI_CHANNEL_MODE_LEFT_TO_STEREO		3
/** Left channel out = right channel in, Right channel out = right channel in.*/
#define HPI_CHANNEL_MODE_RIGHT_TO_STEREO	4
/** Left channel out = (left channel in + right channel in)/2,
    Right channel out = mute. */
#define HPI_CHANNEL_MODE_STEREO_TO_LEFT		5
/** Left channel out = mute,
    Right channel out = (right channel in + left channel in)/2. */
#define HPI_CHANNEL_MODE_STEREO_TO_RIGHT	6
#define HPI_CHANNEL_MODE_LAST			6
/** \} */

/* Bitstream control set attributes */
#define HPI_BITSTREAM_DATA_POLARITY	HPI_CTL_ATTR0(BITSTREAM, 1)
#define HPI_BITSTREAM_CLOCK_EDGE	HPI_CTL_ATTR0(BITSTREAM, 2)
#define HPI_BITSTREAM_CLOCK_SOURCE	HPI_CTL_ATTR0(BITSTREAM, 3)

#define HPI_POLARITY_POSITIVE		0
#define HPI_POLARITY_NEGATIVE		1

/*
Currently fixed at EXTERNAL
#define HPI_SOURCE_EXTERNAL		0
#define HPI_SOURCE_INTERNAL		1
*/

/* Bitstream control get attributes */
#define HPI_BITSTREAM_ACTIVITY		1

/* SampleClock control attributes */
#define HPI_SAMPLECLOCK_SOURCE			HPI_CTL_ATTR0(SAMPLECLOCK, 1)
#define HPI_SAMPLECLOCK_SAMPLERATE		HPI_CTL_ATTR0(SAMPLECLOCK, 2)
#define HPI_SAMPLECLOCK_SOURCE_INDEX		HPI_CTL_ATTR0(SAMPLECLOCK, 3)
#define HPI_SAMPLECLOCK_LOCAL_SAMPLERATE	HPI_CTL_ATTR0(SAMPLECLOCK, 4)

/** \defgroup sampleclock_source SampleClock source values
\{
*/
/** The adapter is clocked from its on-board samplerate generator.
    When this source is set, the adapter samplerate may be set using
    HPI_SampleClock_SetSampleRate(). */
#define HPI_SAMPLECLOCK_SOURCE_ADAPTER		1
/** The adapter is clocked from a dedicated AES/EBU SampleClock input.*/
#define HPI_SAMPLECLOCK_SOURCE_AESEBU_SYNC	2
/** From external connector */
#define HPI_SAMPLECLOCK_SOURCE_WORD		3
/** Board-to-board header */
#define HPI_SAMPLECLOCK_SOURCE_WORD_HEADER	4
/** FUTURE - SMPTE clcok. */
#define HPI_SAMPLECLOCK_SOURCE_SMPTE		5
/** One of the aesebu inputs */
#define HPI_SAMPLECLOCK_SOURCE_AESEBU_INPUT	6
/** The first aesebu input with a valid signal */
#define HPI_SAMPLECLOCK_SOURCE_AESEBU_AUTO	7
/** From the Cobranet interface at either 48 or 96kHz */
#define HPI_SAMPLECLOCK_SOURCE_COBRANET		8
/** From local clock source on module (ASI2416 only) */
#define HPI_SAMPLECLOCK_SOURCE_LOCAL		9
/** From previous adjacent module (ASI2416 only)*/
#define HPI_SAMPLECLOCK_SOURCE_PREV_MODULE	10
/** From the Livewire interface at 48 kHz */
#define HPI_SAMPLECLOCK_SOURCE_LIVEWIRE		11
/*! Update this if you add a new clock source.*/
#define HPI_SAMPLECLOCK_SOURCE_LAST		11
/** \} */

/* Microphone control attributes */
#define HPI_MICROPHONE_PHANTOM_POWER HPI_CTL_ATTR0(MICROPHONE, 1)

/** Equalizer control attributes
*/
/** Used to get number of filters in an EQ. (Can't set) */
#define HPI_EQUALIZER_NUM_FILTERS HPI_CTL_ATTR0(EQUALIZER, 1)
/** Set/get the filter by type, freq, Q, gain */
#define HPI_EQUALIZER_FILTER HPI_CTL_ATTR0(EQUALIZER, 2)
/** Get the biquad coefficients */
#define HPI_EQUALIZER_COEFFICIENTS HPI_CTL_ATTR0(EQUALIZER, 3)

/** Equalizer filter types. Used by HPI_ParametricEQ_SetBand() */
enum HPI_FILTER_TYPE {
	HPI_FILTER_TYPE_BYPASS = 0,	/**< Filter is turned off */

	HPI_FILTER_TYPE_LOWSHELF = 1,	/**< EQ low shelf */
	HPI_FILTER_TYPE_HIGHSHELF = 2,	/**< EQ high shelf */
	HPI_FILTER_TYPE_EQ_BAND = 3,	/**< EQ gain */

	HPI_FILTER_TYPE_LOWPASS = 4,	/**< Standard low pass */
	HPI_FILTER_TYPE_HIGHPASS = 5,	/**< Standard high pass */
	HPI_FILTER_TYPE_BANDPASS = 6,	/**< Standard band pass */
	HPI_FILTER_TYPE_BANDSTOP = 7	/**< Standard band stop/notch */
};

#define HPI_COMPANDER_PARAMS HPI_CTL_ATTR(COMPANDER, 1)

/* Cobranet control attributes.
   MUST be distinct from all other control attributes.
   This is so that host side processing can easily identify a Cobranet control
   and apply additional host side operations (like copying data) as required.
*/
#define HPI_COBRANET_SET	 HPI_CTL_ATTR(COBRANET, 1)
#define HPI_COBRANET_GET	 HPI_CTL_ATTR(COBRANET, 2)
#define HPI_COBRANET_SET_DATA	 HPI_CTL_ATTR(COBRANET, 3)
#define HPI_COBRANET_GET_DATA	 HPI_CTL_ATTR(COBRANET, 4)
#define HPI_COBRANET_GET_STATUS  HPI_CTL_ATTR(COBRANET, 5)
#define HPI_COBRANET_SEND_PACKET HPI_CTL_ATTR(COBRANET, 6)
#define HPI_COBRANET_GET_PACKET  HPI_CTL_ATTR(COBRANET, 7)

/*------------------------------------------------------------
 Cobranet Chip Bridge - copied from HMI.H
------------------------------------------------------------*/
#define  HPI_COBRANET_HMI_cobraBridge		0x20000
#define  HPI_COBRANET_HMI_cobraBridgeTxPktBuf \
	(HPI_COBRANET_HMI_cobraBridge + 0x1000)
#define  HPI_COBRANET_HMI_cobraBridgeRxPktBuf \
	(HPI_COBRANET_HMI_cobraBridge + 0x2000)
#define  HPI_COBRANET_HMI_cobraIfTable1		0x110000
#define  HPI_COBRANET_HMI_cobraIfPhyAddress \
	(HPI_COBRANET_HMI_cobraIfTable1 + 0xd)
#define  HPI_COBRANET_HMI_cobraProtocolIP	0x72000
#define  HPI_COBRANET_HMI_cobraIpMonCurrentIP \
	(HPI_COBRANET_HMI_cobraProtocolIP + 0x0)
#define  HPI_COBRANET_HMI_cobraIpMonStaticIP \
	(HPI_COBRANET_HMI_cobraProtocolIP + 0x2)
#define  HPI_COBRANET_HMI_cobraSys		0x100000
#define  HPI_COBRANET_HMI_cobraSysDesc \
		(HPI_COBRANET_HMI_cobraSys + 0x0)
#define  HPI_COBRANET_HMI_cobraSysObjectID \
	(HPI_COBRANET_HMI_cobraSys + 0x100)
#define  HPI_COBRANET_HMI_cobraSysContact \
	(HPI_COBRANET_HMI_cobraSys + 0x200)
#define  HPI_COBRANET_HMI_cobraSysName \
		(HPI_COBRANET_HMI_cobraSys + 0x300)
#define  HPI_COBRANET_HMI_cobraSysLocation \
	(HPI_COBRANET_HMI_cobraSys + 0x400)

/*------------------------------------------------------------
 Cobranet Chip Status bits
------------------------------------------------------------*/
#define HPI_COBRANET_HMI_STATUS_RXPACKET 2
#define HPI_COBRANET_HMI_STATUS_TXPACKET 3

/*------------------------------------------------------------
 Ethernet header size
------------------------------------------------------------*/
#define HPI_ETHERNET_HEADER_SIZE (16)

/* These defines are used to fill in protocol information for an Ethernet packet
    sent using HMI on CS18102 */
/** ID supplied by Cirrius for ASI packets. */
#define HPI_ETHERNET_PACKET_ID			0x85
/** Simple packet - no special routing required */
#define HPI_ETHERNET_PACKET_V1			0x01
/** This packet must make its way to the host across the HPI interface */
#define HPI_ETHERNET_PACKET_HOSTED_VIA_HMI	0x20
/** This packet must make its way to the host across the HPI interface */
#define HPI_ETHERNET_PACKET_HOSTED_VIA_HMI_V1	0x21
/** This packet must make its way to the host across the HPI interface */
#define HPI_ETHERNET_PACKET_HOSTED_VIA_HPI	0x40
/** This packet must make its way to the host across the HPI interface */
#define HPI_ETHERNET_PACKET_HOSTED_VIA_HPI_V1	0x41

#define HPI_ETHERNET_UDP_PORT (44600)	/*!< UDP messaging port */

/** Base network time out is set to 2 seconds. */
#define HPI_ETHERNET_TIMEOUT_MS      (2000)

/** \defgroup tonedet_attr Tonedetector attributes
\{
Used by HPI_ToneDetector_Set() and HPI_ToneDetector_Get()
*/

/** Set the threshold level of a tonedetector,
Threshold is a -ve number in units of dB/100,
*/
#define HPI_TONEDETECTOR_THRESHOLD HPI_CTL_ATTR(TONEDETECTOR, 1)

/** Get the current state of tonedetection
The result is a bitmap of detected tones.  pairs of bits represent the left
and right channels, with left channel in LSB.
The lowest frequency detector state is in the LSB
*/
#define HPI_TONEDETECTOR_STATE HPI_CTL_ATTR(TONEDETECTOR, 2)

/** Get the frequency of a tonedetector band.
*/
#define HPI_TONEDETECTOR_FREQUENCY HPI_CTL_ATTR(TONEDETECTOR, 3)

/**\}*/

/** \defgroup silencedet_attr SilenceDetector attributes
\{
*/

/** Get the current state of tonedetection
The result is a bitmap with 1s for silent channels. Left channel is in LSB
*/
#define HPI_SILENCEDETECTOR_STATE \
  HPI_CTL_ATTR(SILENCEDETECTOR, 2)

/** Set the threshold level of a SilenceDetector,
Threshold is a -ve number in units of dB/100,
*/
#define HPI_SILENCEDETECTOR_THRESHOLD \
  HPI_CTL_ATTR(SILENCEDETECTOR, 1)

/** get/set the silence time before the detector triggers
*/
#define HPI_SILENCEDETECTOR_DELAY \
       HPI_CTL_ATTR(SILENCEDETECTOR, 3)

/**\}*/

/******************************************* CONTROLX ATTRIBUTES ****/
/* NOTE: All controlx attributes must be unique, unlike control attributes */
/******************************************* ASYNC ATTRIBUTES ****/
/** \defgroup async_event Async Event sources
\{
*/
#define HPI_ASYNC_EVENT_GPIO		1	/**< GPIO event. */
#define HPI_ASYNC_EVENT_SILENCE		2	/**< Silence event detected. */
#define HPI_ASYNC_EVENT_TONE		3	/**< tone event detected. */
/** \} */

/*******************************************/
/** \defgroup errorcodes HPI Error codes

Almost all HPI functions return an error code
A return value of zero means there was no error.
Otherwise one of these error codes is returned.
Error codes can be converted to a descriptive string using HPI_GetErrorText()

\note When a new error code is added HPI_GetErrorText() MUST be updated.
\note codes 1-100 are reserved for driver use
\{
*/
/** Message type does not exist. */
#define HPI_ERROR_INVALID_TYPE		100
/** Object type does not exist. */
#define HPI_ERROR_INVALID_OBJ		101
/** Function does not exist. */
#define HPI_ERROR_INVALID_FUNC		102
/** The specified object (adapter/Stream) does not exist. */
#define HPI_ERROR_INVALID_OBJ_INDEX	103
/** Trying to access an object that has not been opened yet. */
#define HPI_ERROR_OBJ_NOT_OPEN		104
/** Trying to open an already open object. */
#define HPI_ERROR_OBJ_ALREADY_OPEN	105
/** PCI, ISA resource not valid. */
#define HPI_ERROR_INVALID_RESOURCE	106
/** GetInfo call from SubSysFindAdapters failed. */
#define HPI_ERROR_SUBSYSFINDADAPTERS_GETINFO	107
/** Default response was never updated with actual error code. */
#define HPI_ERROR_INVALID_RESPONSE	108
/** wSize field of response was not updated,
    indicating that the message was not processed. */
#define HPI_ERROR_PROCESSING_MESSAGE	109
/** The network did not respond in a timely manner. */
#define HPI_ERROR_NETWORK_TIMEOUT	110
/** An HPI handle is invalid (uninitialised?). */
#define HPI_ERROR_INVALID_HANDLE	111
/** A function or attribute has not been implemented yet. */
#define HPI_ERROR_UNIMPLEMENTED		112

/** Too many adapters.*/
#define HPI_ERROR_TOO_MANY_ADAPTERS	200
/** Bad adpater. */
#define HPI_ERROR_BAD_ADAPTER		201
/** Adapter number out of range or not set properly. */
#define HPI_ERROR_BAD_ADAPTER_NUMBER	202
/** 2 adapters with the same adapter number. */
#define HPI_DUPLICATE_ADAPTER_NUMBER	203
/** DSP code failed to bootload. */
#define HPI_ERROR_DSP_BOOTLOAD		204
/** Adapter failed DSP code self test. */
#define HPI_ERROR_DSP_SELFTEST		205
/** Couldn't find or open the DSP code file. */
#define HPI_ERROR_DSP_FILE_NOT_FOUND	206
/** Internal DSP hardware error. */
#define HPI_ERROR_DSP_HARDWARE		207
/** Could not allocate memory in DOS. */
#define HPI_ERROR_DOS_MEMORY_ALLOC	208
/** Could not allocate memory */
#define HPI_ERROR_MEMORY_ALLOC		208
/** Failed to correctly load/config PLD .*/
#define HPI_ERROR_PLD_LOAD		209
/** Unexpected end of file, block length too big etc. */
#define HPI_ERROR_DSP_FILE_FORMAT	210

/** Found but could not open DSP code file. */
#define HPI_ERROR_DSP_FILE_ACCESS_DENIED 211
/** First DSP code section header not found in DSP file. */
#define HPI_ERROR_DSP_FILE_NO_HEADER	212
/** File read operation on DSP code file failed. */
#define HPI_ERROR_DSP_FILE_READ_ERROR	213
/** DSP code for adapter family not found. */
#define HPI_ERROR_DSP_SECTION_NOT_FOUND 214
/** Other OS specific error opening DSP file. */
#define HPI_ERROR_DSP_FILE_OTHER_ERROR	215
/** Sharing violation opening DSP code file. */
#define HPI_ERROR_DSP_FILE_SHARING_VIOLATION	216
/** DSP code section header had size == 0. */
#define HPI_ERROR_DSP_FILE_NULL_HEADER	217

/** Base number for flash errors. */
#define HPI_ERROR_FLASH				220

/** Flash has bad checksum */
#define HPI_ERROR_BAD_CHECKSUM (HPI_ERROR_FLASH+1)
#define HPI_ERROR_BAD_SEQUENCE (HPI_ERROR_FLASH+2)
#define HPI_ERROR_FLASH_ERASE (HPI_ERROR_FLASH+3)
#define HPI_ERROR_FLASH_PROGRAM (HPI_ERROR_FLASH+4)
#define HPI_ERROR_FLASH_VERIFY (HPI_ERROR_FLASH+5)
#define HPI_ERROR_FLASH_TYPE (HPI_ERROR_FLASH+6)
#define HPI_ERROR_FLASH_START (HPI_ERROR_FLASH+7)

/** Reserved for OEMs. */
#define HPI_ERROR_RESERVED_1		290

/** Stream does not exist. */
#define HPI_ERROR_INVALID_STREAM	300
/** Invalid compression format. */
#define HPI_ERROR_INVALID_FORMAT	301
/** Invalid format samplerate */
#define HPI_ERROR_INVALID_SAMPLERATE	302
/** Invalid format number of channels. */
#define HPI_ERROR_INVALID_CHANNELS	303
/** Invalid format bitrate. */
#define HPI_ERROR_INVALID_BITRATE	304
/** Invalid datasize used for stream read/write. */
#define HPI_ERROR_INVALID_DATASIZE	305
/** Stream buffer is full during stream write. */
#define HPI_ERROR_BUFFER_FULL		306
/** Stream buffer is empty during stream read. */
#define HPI_ERROR_BUFFER_EMPTY		307
/** Invalid datasize used for stream read/write. */
#define HPI_ERROR_INVALID_DATA_TRANSFER 308

/** Object can't do requested operation in its current
state, e.g. set format, change rec mux state while recording.*/
#define HPI_ERROR_INVALID_OPERATION	310

/** Where an SRG is shared amongst streams, an incompatible samplerate is one
that is different to any currently playing or recording stream. */
#define HPI_ERROR_INCOMPATIBLE_SAMPLERATE 311
/** Adapter mode is illegal.*/
#define HPI_ERROR_BAD_ADAPTER_MODE	312

/** There have been too many attempts to set the adapter's
capabilities (using bad keys). The card should be returned
to ASI if further capabilities updates are required */
#define HPI_ERROR_TOO_MANY_CAPABILITY_CHANGE_ATTEMPTS 313
/** Streams on different adapters cannot be grouped. */
#define HPI_ERROR_NO_INTERADAPTER_GROUPS 314
/** Streams on different DSPs cannot be grouped. */
#define HPI_ERROR_NO_INTERDSP_GROUPS	315

/** Mixer controls */
/** Invalid mixer node for this adapter. */
#define HPI_ERROR_INVALID_NODE		400
/** Invalid control. */
#define HPI_ERROR_INVALID_CONTROL	401
/** Invalid control value was passed. */
#define HPI_ERROR_INVALID_CONTROL_VALUE 402
/** Control attribute not supported by this control. */
#define HPI_ERROR_INVALID_CONTROL_ATTRIBUTE	403
/** Control is disbaled. */
#define HPI_ERROR_CONTROL_DISABLED	404
/** I2C transaction failed due to a missing ACK. */
#define HPI_ERROR_CONTROL_I2C_MISSING_ACK	405
/** Control attribute is valid, but not supported by this hardware. */
#define HPI_ERROR_UNSUPPORTED_CONTROL_ATTRIBUTE 406
/** Control is busy, or coming out of
    reset and cannot be accessed at this time. */
#define HPI_ERROR_CONTROL_NOT_READY	407

/** Non volatile memory */
#define HPI_ERROR_NVMEM_BUSY		450
#define HPI_ERROR_NVMEM_FULL		451
#define HPI_ERROR_NVMEM_FAIL		452

/** I2C */
#define HPI_ERROR_I2C_MISSING_ACK	HPI_ERROR_CONTROL_I2C_MISSING_ACK
#define HPI_ERROR_I2C_BAD_ADR		460

/** custom error to use for debugging */
#define HPI_ERROR_CUSTOM		600

/** hpioct32.c can't obtain mutex */
#define HPI_ERROR_MUTEX_TIMEOUT		700

/** errors from HPI backends have values >= this */
#define HPI_ERROR_BACKEND_BASE		900

/** indicates a cached u16 value is invalid. */
#define HPI_ERROR_ILLEGAL_CACHE_VALUE	0xffff
/**\}*/

/* maximums */
/** Maximum number of adapters per HPI sub-system
   WARNING: modifying this value changes the response structure size.*/
#define HPI_MAX_ADAPTERS		20
/** Maximum number of in or out streams per adapter */
#define HPI_MAX_STREAMS			16
#define HPI_MAX_CHANNELS		2	/* per stream */
#define HPI_MAX_NODES			8	/* per mixer ? */
#define HPI_MAX_CONTROLS		4	/* per node ? */
/** maximum number of ancillary bytes per MPEG frame */
#define HPI_MAX_ANC_BYTES_PER_FRAME	(64)
#define HPI_STRING_LEN			16

/* units */
#define HPI_OSTREAM_VELOCITY_UNITS	4096
#define HPI_OSTREAM_TIMESCALE_UNITS	10000

/**\}*/
/* end group hpi_defines */

/* ////////////////////////////////////////////////////////////////////// */
/* STRUCTURES */
#ifndef DISABLE_PRAGMA_PACK1
#pragma pack(push, 1)
#endif

/** Structure containing sample format information.
    See also HPI_FormatCreate().
  */
struct hpi_format {
	u32 dwSampleRate;
				/**< 11025, 32000, 44100 ... */
	u32 dwBitRate;	       /**< for MPEG */
	u32 dwAttributes;
				/**< Stereo/JointStereo/Mono */
	u16 wModeLegacy;
				/**< Legacy ancillary mode or idle bit	*/
	u16 wUnused;	       /**< Unused */
	u16 wChannels; /**< 1,2..., (or ancillary mode or idle bit */
	u16 wFormat;   /**< HPI_FORMAT_PCM16, _MPEG etc. See \ref formats. */
};

struct hpi_anc_frame {
	u32 dwValidBitsInThisFrame;
	u8 bData[HPI_MAX_ANC_BYTES_PER_FRAME];
};

struct hpi_punchinout {
	u32 dwPunchInSample;
	u32 dwPunchOutSample;
};

struct hpi_streamid {
	u16 wObjectType;
		    /**< Type of object, HPI_OBJ_OSTREAM or HPI_OBJ_ISTREAM. */
	u16 wStreamIndex; /**< OStream or IStream index. */
};

/** An object for containing a single async event.
*/
struct hpi_async_event {
	u16 wEventType;	/**< Type of event. \sa async_event  */
	u16 wSequence;	/**< Sequence number, allows lost event detection */
	u32 dwState;	/**< New state */
	u32 hObject;	/**< Handle to the object returning the event. */
	union {
		struct {
			u16 wIndex; /**< GPIO bit index. */
		} gpio;
		struct {
			u16 wNodeIndex;	/**< What node is the control on ? */
			u16 wNodeType;	/**< What type of node is the control on ? */
		} control;
	} u;
};

/* skip host side function declarations for
   DSP compile and documentation extraction */

struct hpi_hsubsys {
	int not_really_used;
};

#ifndef DISABLE_PRAGMA_PACK1
#pragma pack(pop)
#endif

/*////////////////////////////////////////////////////////////////////////// */
/* HPI FUNCTIONS */

/*/////////////////////////// */
/* DATA and FORMAT and STREAM */

u16 HPI_StreamEstimateBufferSize(
	struct hpi_format *pF,
	u32 dwHostPollingRateInMilliSeconds,
	u32 *dwRecommendedBufferSize
);

/*/////////// */
/* SUB SYSTEM */
struct hpi_hsubsys *HPI_SubSysCreate(
	void
);

void HPI_SubSysFree(
	struct hpi_hsubsys *phSubSys
);

u16 HPI_SubSysGetVersion(
	struct hpi_hsubsys *phSubSys,
	u32 *pdwVersion
);

u16 HPI_SubSysGetVersionEx(
	struct hpi_hsubsys *phSubSys,
	u32 *pdwVersionEx
);

u16 HPI_SubSysGetInfo(
	struct hpi_hsubsys *phSubSys,
	u32 *pdwVersion,
	u16 *pwNumAdapters,
	u16 awAdapterList[],
	u16 wListLength
);

/* SGT added 3-2-97 */
u16 HPI_SubSysFindAdapters(
	struct hpi_hsubsys *phSubSys,
	u16 *pwNumAdapters,
	u16 awAdapterList[],
	u16 wListLength
);

u16 HPI_SubSysGetNumAdapters(
	struct hpi_hsubsys *phSubSys,
	int *pnNumAdapters
);

u16 HPI_SubSysGetAdapter(
	struct hpi_hsubsys *phSubSys,
	int nIterator,
	u32 *pdwAdapterIndex,
	u16 *pwAdapterType
);

u16 HPI_SubSysSsx2Bypass(
	struct hpi_hsubsys *phSubSys,
	u16 wBypass
);

u16 HPI_SubSysSetHostNetworkInterface(
	struct hpi_hsubsys *phSubSys,
	char *szInterface
);

/*///////// */
/* ADAPTER */

u16 HPI_AdapterOpen(
	struct hpi_hsubsys *phSubSys,
	u16 wAdapterIndex
);

u16 HPI_AdapterClose(
	struct hpi_hsubsys *phSubSys,
	u16 wAdapterIndex
);

u16 HPI_AdapterGetInfo(
	struct hpi_hsubsys *phSubSys,
	u16 wAdapterIndex,
	u16 *pwNumOutStreams,
	u16 *pwNumInStreams,
	u16 *pwVersion,
	u32 *pdwSerialNumber,
	u16 *pwAdapterType
);

u16 HPI_AdapterGetModuleByIndex(
	struct hpi_hsubsys *phSubSys,
	u16 wAdapterIndex,
	u16 wModuleIndex,
	u16 *pwNumOutputs,
	u16 *pwNumInputs,
	u16 *pwVersion,
	u32 *pdwSerialNumber,
	u16 *pwModuleType,
	u32 *phModule
);

u16 HPI_AdapterSetMode(
	struct hpi_hsubsys *phSubSys,
	u16 wAdapterIndex,
	u32 dwAdapterMode
);
u16 HPI_AdapterSetModeEx(
	struct hpi_hsubsys *phSubSys,
	u16 wAdapterIndex,
	u32 dwAdapterMode,
	u16 wQueryOrSet
);

u16 HPI_AdapterGetMode(
	struct hpi_hsubsys *phSubSys,
	u16 wAdapterIndex,
	u32 *pdwAdapterMode
);

u16 HPI_AdapterGetAssert(
	struct hpi_hsubsys *phSubSys,
	u16 wAdapterIndex,
	u16 *wAssertPresent,
	char *pszAssert,
	u16 *pwLineNumber
);

u16 HPI_AdapterGetAssertEx(
	struct hpi_hsubsys *phSubSys,
	u16 wAdapterIndex,
	u16 *wAssertPresent,
	char *pszAssert,
	u32 *pdwLineNumber,
	u16 *pwAssertOnDsp
);

u16 HPI_AdapterTestAssert(
	struct hpi_hsubsys *phSubSys,
	u16 wAdapterIndex,
	u16 wAssertId
);

u16 HPI_AdapterEnableCapability(
	struct hpi_hsubsys *phSubSys,
	u16 wAdapterIndex,
	u16 wCapability,
	u32 dwKey
);

u16 HPI_AdapterSelfTest(
	struct hpi_hsubsys *phSubSys,
	u16 wAdapterIndex
);

u16 HPI_AdapterSetProperty(
	struct hpi_hsubsys *phSubSys,
	u16 wAdapterIndex,
	u16 wProperty,
	u16 wParamter1,
	u16 wParamter2
);

u16 HPI_AdapterGetProperty(
	struct hpi_hsubsys *phSubSys,
	u16 wAdapterIndex,
	u16 wProperty,
	u16 *pwParamter1,
	u16 *pwParamter2
);

u16 HPI_AdapterFindObject(
	const struct hpi_hsubsys *phSubSys,
	u16 wAdapterIndex,
	u16 wObjectType,
	u16 wObjectIndex,
	u16 *pDspIndex
);

u16 HPI_AdapterEnumerateProperty(
	struct hpi_hsubsys *phSubSys,
	u16 wAdapterIndex,
	u16 wIndex,
	u16 wWhatToEnumerate,
	u16 wPropertyIndex,
	u32 *pdwSetting
);

/*////////////// */
/* NonVol Memory */
u16 HPI_NvMemoryOpen(
	struct hpi_hsubsys *phSubSys,
	u16 wAdapterIndex,
	u32 *phNvMemory,
	u16 *pwSizeInBytes
);

u16 HPI_NvMemoryReadByte(
	struct hpi_hsubsys *phSubSys,
	u32 hNvMemory,
	u16 wIndex,
	u16 *pwData
);

u16 HPI_NvMemoryWriteByte(
	struct hpi_hsubsys *phSubSys,
	u32 hNvMemory,
	u16 wIndex,
	u16 wData
);

/*////////////// */
/* Digital I/O */
u16 HPI_GpioOpen(
	struct hpi_hsubsys *phSubSys,
	u16 wAdapterIndex,
	u32 *phGpio,
	u16 *pwNumberInputBits,
	u16 *pwNumberOutputBits
);

u16 HPI_GpioReadBit(
	struct hpi_hsubsys *phSubSys,
	u32 hGpio,
	u16 wBitIndex,
	u16 *pwBitData
);

u16 HPI_GpioReadAllBits(
	struct hpi_hsubsys *phSubSys,
	u32 hGpio,
	u16 *pwBitData
);

u16 HPI_GpioWriteBit(
	struct hpi_hsubsys *phSubSys,
	u32 hGpio,
	u16 wBitIndex,
	u16 wBitData
);

/**********************/
/* Async Event Object */
/**********************/
u16 HPI_AsyncEventOpen(
	struct hpi_hsubsys *phSubSys,
	u16 wAdapterIndex,
	u32 *phAsync
);

u16 HPI_AsyncEventClose(
	struct hpi_hsubsys *phSubSys,
	u32 hAsync
);

u16 HPI_AsyncEventWait(
	struct hpi_hsubsys *phSubSys,
	u32 hAsync,
	u16 wMaximumEvents,
	struct hpi_async_event *pEvents,
	u16 *pwNumberReturned
);

u16 HPI_AsyncEventGetCount(
	struct hpi_hsubsys *phSubSys,
	u32 hAsync,
	u16 *pwCount
);

u16 HPI_AsyncEventGet(
	struct hpi_hsubsys *phSubSys,
	u32 hAsync,
	u16 wMaximumEvents,
	struct hpi_async_event *pEvents,
	u16 *pwNumberReturned
);

/*/////////// */
/* WATCH-DOG  */
u16 HPI_WatchdogOpen(
	struct hpi_hsubsys *phSubSys,
	u16 wAdapterIndex,
	u32 *phWatchdog
);

u16 HPI_WatchdogSetTime(
	struct hpi_hsubsys *phSubSys,
	u32 hWatchdog,
	u32 dwTimeMillisec
);

u16 HPI_WatchdogPing(
	struct hpi_hsubsys *phSubSys,
	u32 hWatchdog
);

/**************/
/* OUT STREAM */
/**************/
u16 HPI_OutStreamOpen(
	struct hpi_hsubsys *phSubSys,
	u16 wAdapterIndex,
	u16 wOutStreamIndex,
	u32 *phOutStream
);

u16 HPI_OutStreamClose(
	struct hpi_hsubsys *phSubSys,
	u32 hOutStream
);

u16 HPI_OutStreamGetInfoEx(
	struct hpi_hsubsys *phSubSys,
	u32 hOutStream,
	u16 *pwState,
	u32 *pdwBufferSize,
	u32 *pdwDataToPlay,
	u32 *pdwSamplesPlayed,
	u32 *pdwAuxiliaryDataToPlay
);

u16 HPI_OutStreamWriteBuf(
	struct hpi_hsubsys *phSubSys,
	u32 hOutStream,
	u8 *pbWriteBuf,
	u32 dwBytesToWrite,
	struct hpi_format *pFormat
);

u16 HPI_OutStreamStart(
	struct hpi_hsubsys *phSubSys,
	u32 hOutStream
);

u16 HPI_OutStreamStop(
	struct hpi_hsubsys *phSubSys,
	u32 hOutStream
);

u16 HPI_OutStreamSinegen(
	struct hpi_hsubsys *phSubSys,
	u32 hOutStream
);

u16 HPI_OutStreamReset(
	struct hpi_hsubsys *phSubSys,
	u32 hOutStream
);

u16 HPI_OutStreamQueryFormat(
	struct hpi_hsubsys *phSubSys,
	u32 hOutStream,
	struct hpi_format *pFormat
);

u16 HPI_OutStreamSetPunchInOut(
	struct hpi_hsubsys *phSubSys,
	u32 hOutStream,
	u32 dwPunchInSample,
	u32 dwPunchOutSample
);

u16 HPI_OutStreamSetVelocity(
	struct hpi_hsubsys *phSubSys,
	u32 hOutStream,
	short nVelocity
);

u16 HPI_OutStreamAncillaryReset(
	struct hpi_hsubsys *phSubSys,
	u32 hOutStream,
	u16 wMode
);

u16 HPI_OutStreamAncillaryGetInfo(
	struct hpi_hsubsys *phSubSys,
	u32 hOutStream,
	u32 *pdwFramesAvailable
);

u16 HPI_OutStreamAncillaryRead(
	struct hpi_hsubsys *phSubSys,
	u32 hOutStream,
	struct hpi_anc_frame *pAncFrameBuffer,
	u32 dwAncFrameBufferSizeInBytes,
	u32 dwNumberOfAncillaryFramesToRead
);

u16 HPI_OutStreamSetTimeScale(
	struct hpi_hsubsys *phSubSys,
	u32 hOutStream,
	u32 dwTimeScaleX10000
);

u16 HPI_OutStreamHostBufferAllocate(
	struct hpi_hsubsys *phSubSys,
	u32 hOutStream,
	u32 dwSizeInBytes
);

u16 HPI_OutStreamHostBufferFree(
	struct hpi_hsubsys *phSubSys,
	u32 hOutStream
);

u16 HPI_OutStreamGroupAdd(
	struct hpi_hsubsys *phSubSys,
	u32 hOutStream,
	u32 hStream
);

u16 HPI_OutStreamGroupGetMap(
	struct hpi_hsubsys *phSubSys,
	u32 hOutStream,
	u32 *pdwOutStreamMap,
	u32 *pdwInStreamMap
);

u16 HPI_OutStreamGroupReset(
	struct hpi_hsubsys *phSubSys,
	u32 hOutStream
);

/*////////// */
/* IN_STREAM */
u16 HPI_InStreamOpen(
	struct hpi_hsubsys *phSubSys,
	u16 wAdapterIndex,
	u16 wInStreamIndex,
	u32 *phInStream
);

u16 HPI_InStreamClose(
	struct hpi_hsubsys *phSubSys,
	u32 hInStream
);

u16 HPI_InStreamQueryFormat(
	struct hpi_hsubsys *phSubSys,
	u32 hInStream,
	struct hpi_format *pFormat
);

u16 HPI_InStreamSetFormat(
	struct hpi_hsubsys *phSubSys,
	u32 hInStream,
	struct hpi_format *pFormat
);

u16 HPI_InStreamReadBuf(
	struct hpi_hsubsys *phSubSys,
	u32 hInStream,
	u8 *pbReadBuf,
	u32 dwBytesToRead
);

u16 HPI_InStreamStart(
	struct hpi_hsubsys *phSubSys,
	u32 hInStream
);

u16 HPI_InStreamStop(
	struct hpi_hsubsys *phSubSys,
	u32 hInStream
);

u16 HPI_InStreamReset(
	struct hpi_hsubsys *phSubSys,
	u32 hInStream
);

u16 HPI_InStreamGetInfoEx(
	struct hpi_hsubsys *phSubSys,
	u32 hInStream,
	u16 *pwState,
	u32 *pdwBufferSize,
	u32 *pdwDataRecorded,
	u32 *pdwSamplesRecorded,
	u32 *pdwAuxiliaryDataRecorded
);

u16 HPI_InStreamAncillaryReset(
	struct hpi_hsubsys *phSubSys,
	u32 hInStream,
	u16 wBytesPerFrame,
	u16 wMode,
	u16 wAlignment,
	u16 wIdleBit
);

u16 HPI_InStreamAncillaryGetInfo(
	struct hpi_hsubsys *phSubSys,
	u32 hInStream,
	u32 *pdwFrameSpace
);

u16 HPI_InStreamAncillaryWrite(
	struct hpi_hsubsys *phSubSys,
	u32 hInStream,
	struct hpi_anc_frame *pAncFrameBuffer,
	u32 dwAncFrameBufferSizeInBytes,
	u32 dwNumberOfAncillaryFramesToWrite
);

u16 HPI_InStreamHostBufferAllocate(
	struct hpi_hsubsys *phSubSys,
	u32 hInStream,
	u32 dwSizeInBytes
);

u16 HPI_InStreamHostBufferFree(
	struct hpi_hsubsys *phSubSys,
	u32 hInStream
);

u16 HPI_InStreamGroupAdd(
	struct hpi_hsubsys *phSubSys,
	u32 hInStream,
	u32 hStream
);

u16 HPI_InStreamGroupGetMap(
	struct hpi_hsubsys *phSubSys,
	u32 hInStream,
	u32 *pdwOutStreamMap,
	u32 *pdwInStreamMap
);

u16 HPI_InStreamGroupReset(
	struct hpi_hsubsys *phSubSys,
	u32 hInStream
);

/*********/
/* MIXER */
/*********/
u16 HPI_MixerOpen(
	struct hpi_hsubsys *phSubSys,
	u16 wAdapterIndex,
	u32 *phMixer
);

u16 HPI_MixerClose(
	struct hpi_hsubsys *phSubSys,
	u32 hMixer
);

u16 HPI_MixerGetControl(
	struct hpi_hsubsys *phSubSys,
	u32 hMixer,
	u16 wSrcNodeType,
	u16 wSrcNodeTypeIndex,
	u16 wDstNodeType,
	u16 wDstNodeTypeIndex,
	u16 wControlType,
	u32 *phControl
);

u16 HPI_MixerGetControlByIndex(
	struct hpi_hsubsys *phSubSys,
	u32 hMixer,
	u16 wControlIndex,
	u16 *pwSrcNodeType,
	u16 *pwSrcNodeIndex,
	u16 *pwDstNodeType,
	u16 *pwDstNodeIndex,
	u16 *pwControlType,
	u32 *phControl
);

u16 HPI_MixerStore(
	struct hpi_hsubsys *phSubSys,
	u32 hMixer,
	enum HPI_MIXER_STORE_COMMAND Command,
	u16 wIndex
);
/*************************/
/* mixer CONTROLS		 */
/*************************/

/* Generic query of available control settings */
u16 HPI_ControlQuery(
	const struct hpi_hsubsys *phSubSys,
	const u32 hControl,
	const u16 wAttrib,
	const u32 dwIndex,
	const u32 dwParam,
	u32 *pdwSetting
);

#ifndef SWIG
/* Generic setting of control attribute value */
u16 HPI_ControlParamSet(
	const struct hpi_hsubsys *phSubSys,
	const u32 hControl,
	const u16 wAttrib,
	const u32 dwParam1,
	const u32 dwParam2
);

/* generic getting of control attribute value.
   Null pointers allowed for return values
*/
u16 HPI_ControlParamGet(
	const struct hpi_hsubsys *phSubSys,
	const u32 hControl,
	const u16 wAttrib,
	u32 dwParam1,
	u32 dwParam2,
	u32 *pdwParam1,
	u32 *pdwParam2
);
#endif

/*************************/
/* volume control		 */
/*************************/
u16 HPI_VolumeSetGain(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	short anGain0_01dB[HPI_MAX_CHANNELS]
);

u16 HPI_VolumeGetGain(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	short anGain0_01dB_out[HPI_MAX_CHANNELS]
);

#define HPI_VolumeGetRange HPI_VolumeQueryRange
u16 HPI_VolumeQueryRange(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	short *nMinGain_01dB,
	short *nMaxGain_01dB,
	short *nStepGain_01dB
);

u16 HPI_VolumeAutoFade(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	short anStopGain0_01dB[HPI_MAX_CHANNELS],
	u32 wDurationMs
);

u16 HPI_VolumeAutoFadeProfile(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	short anStopGain0_01dB[HPI_MAX_CHANNELS],
	u32 dwDurationMs,
	u16 dwProfile
);

/*************************/
/* level control		 */
/*************************/
u16 HPI_LevelSetGain(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	short anGain0_01dB[HPI_MAX_CHANNELS]
);

u16 HPI_LevelGetGain(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	short anGain0_01dB_out[HPI_MAX_CHANNELS]
);

/*************************/
/* meter control		 */
/*************************/
u16 HPI_MeterGetPeak(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	short anPeak0_01dB_out[HPI_MAX_CHANNELS]
);

u16 HPI_MeterGetRms(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	short anPeak0_01dB_out[HPI_MAX_CHANNELS]
);

u16 HPI_MeterSetPeakBallistics(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u16 nAttack,
	u16 nDecay
);

u16 HPI_MeterSetRmsBallistics(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u16 nAttack,
	u16 nDecay
);

u16 HPI_MeterGetPeakBallistics(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u16 *nAttack,
	u16 *nDecay
);

u16 HPI_MeterGetRmsBallistics(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u16 *nAttack,
	u16 *nDecay
);

/*************************/
/* channel mode control  */
/*************************/
u16 HPI_ChannelModeSet(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u16 wMode
);

u16 HPI_ChannelModeGet(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u16 *wMode
);

/*************************/
/* Tuner control		 */
/*************************/
u16 HPI_Tuner_SetBand(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u16 wBand
);

u16 HPI_Tuner_GetBand(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u16 *pwBand
);

u16 HPI_Tuner_SetFrequency(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u32 wFreqInkHz
);

u16 HPI_Tuner_GetFrequency(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u32 *pwFreqInkHz
);

u16 HPI_Tuner_GetRFLevel(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	short *pwLevel
);

u16 HPI_Tuner_GetRawRFLevel(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	short *pwLevel
);

u16 HPI_Tuner_SetGain(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	short nGain
);

u16 HPI_Tuner_GetGain(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	short *pnGain
);

u16 HPI_Tuner_GetStatus(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u16 *pwStatusMask,
	u16 *pwStatus
);

u16 HPI_Tuner_SetMode(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u32 nMode,
	u32 nValue
);

u16 HPI_Tuner_GetMode(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u32 nMode,
	u32 *pnValue
);

u16 HPI_Tuner_GetRDS(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	char *pRdsData
);

/****************************/
/* AES/EBU Receiver control */
/****************************/
u16 HPI_AESEBU_Receiver_SetFormat(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u16 wSource
);

u16 HPI_AESEBU_Receiver_GetFormat(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u16 *pwSource
);

u16 HPI_AESEBU_Receiver_GetSampleRate(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u32 *pdwSampleRate
);

u16 HPI_AESEBU_Receiver_GetUserData(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u16 wIndex,
	u16 *pwData
);

u16 HPI_AESEBU_Receiver_GetChannelStatus(
	struct hpi_hsubsys
	*phSubSys,
	u32 hControl,
	u16 wIndex,
	u16 *pwData
);

u16 HPI_AESEBU_Receiver_GetErrorStatus(
	struct hpi_hsubsys
	*phSubSys,
	u32 hControl,
	u16 *pwErrorData
);

/*******************************/
/* AES/EBU Transmitter control */
/*******************************/
u16 HPI_AESEBU_Transmitter_SetSampleRate(
	struct hpi_hsubsys
	*phSubSys,
	u32 hControl,
	u32 dwSampleRate
);

u16 HPI_AESEBU_Transmitter_SetUserData(
	struct hpi_hsubsys
	*phSubSys,
	u32 hControl,
	u16 wIndex,
	u16 wData
);

u16 HPI_AESEBU_Transmitter_SetChannelStatus(
	struct hpi_hsubsys
	*phSubSys,
	u32 hControl,
	u16 wIndex,
	u16 wData
);

u16 HPI_AESEBU_Transmitter_GetChannelStatus(
	struct hpi_hsubsys
	*phSubSys,
	u32 hControl,
	u16 wIndex,
	u16 *pwData
);

u16 HPI_AESEBU_Transmitter_SetFormat(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u16 wOutputFormat
);

u16 HPI_AESEBU_Transmitter_GetFormat(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u16 *pwOutputFormat
);

/***********************/
/* multiplexer control */
/***********************/
u16 HPI_Multiplexer_SetSource(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u16 wSourceNodeType,
	u16 wSourceNodeIndex
);

u16 HPI_Multiplexer_GetSource(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u16 *wSourceNodeType,
	u16 *wSourceNodeIndex
);

u16 HPI_Multiplexer_QuerySource(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u16 nIndex,
	u16 *wSourceNodeType,
	u16 *wSourceNodeIndex
);

/***************/
/* VOX control */
/***************/
u16 HPI_VoxSetThreshold(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	short anGain0_01dB
);

u16 HPI_VoxGetThreshold(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	short *anGain0_01dB
);

/*********************/
/* Bitstream control */
/*********************/
u16 HPI_Bitstream_SetClockEdge(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u16 wEdgeType
);

u16 HPI_Bitstream_SetDataPolarity(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u16 wPolarity
);

u16 HPI_Bitstream_GetActivity(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u16 *pwClkActivity,
	u16 *pwDataActivity
);

/***********************/
/* SampleClock control */
/***********************/
u16 HPI_SampleClock_SetSource(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u16 wSource
);

u16 HPI_SampleClock_GetSource(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u16 *pwSource
);

u16 HPI_SampleClock_SetSourceIndex(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u16 wSourceIndex
);

u16 HPI_SampleClock_GetSourceIndex(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u16 *pwSourceIndex
);

u16 HPI_SampleClock_SetSampleRate(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u32 dwSampleRate
);

u16 HPI_SampleClock_GetSampleRate(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u32 *pdwSampleRate
);

/***********************/
/* Microphone control */
/***********************/
u16 HPI_Microphone_SetPhantomPower(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u16 wOnOff
);

u16 HPI_Microphone_GetPhantomPower(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u16 *pwOnOff
);

/*******************************
  Parametric Equalizer control
*******************************/
u16 HPI_ParametricEQ_GetInfo(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u16 *pwNumberOfBands,
	u16 *pwEnabled
);

u16 HPI_ParametricEQ_SetState(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u16 wOnOff
);

u16 HPI_ParametricEQ_SetBand(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u16 wIndex,
	u16 nType,
	u32 dwFrequencyHz,
	short nQ100,
	short nGain0_01dB
);

u16 HPI_ParametricEQ_GetBand(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u16 wIndex,
	u16 *pnType,
	u32 *pdwFrequencyHz,
	short *pnQ100,
	short *pnGain0_01dB
);

u16 HPI_ParametricEQ_GetCoeffs(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u16 wIndex,
	short coeffs[5]
);

/*******************************
  Compressor Expander control
*******************************/

u16 HPI_Compander_Set(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u16 wAttack,
	u16 wDecay,
	short wRatio100,
	short nThreshold0_01dB,
	short nMakeupGain0_01dB
);

u16 HPI_Compander_Get(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u16 *pwAttack,
	u16 *pwDecay,
	short *pwRatio100,
	short *pnThreshold0_01dB,
	short *pnMakeupGain0_01dB
);

/*******************************
  Cobranet HMI control
*******************************/
u16 HPI_Cobranet_HmiWrite(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u32 dwHmiAddress,
	u32 dwByteCount,
	u8 *pbData
);

u16 HPI_Cobranet_HmiRead(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u32 dwHmiAddress,
	u32 dwMaxByteCount,
	u32 *pdwByteCount,
	u8 *pbData
);

u16 HPI_Cobranet_HmiGetStatus(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u32 *pdwStatus,
	u32 *pdwReadableSize,
	u32 *pdwWriteableSize
);

/*Read the current IP address
*/
u16 HPI_Cobranet_GetIPaddress(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u32 *pdwIPaddress
);
/* Write the current IP address
*/
u16 HPI_Cobranet_SetIPaddress(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u32 dwIPaddress
);
/* Read the static IP address
*/
u16 HPI_Cobranet_GetStaticIPaddress(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u32 *pdwIPaddress
);
/* Write the static IP address
*/
u16 HPI_Cobranet_SetStaticIPaddress(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u32 dwIPaddress
);
/* Read the MAC address
*/
u16 HPI_Cobranet_GetMACaddress(
	struct hpi_hsubsys *phSubSys,
	u32 hControl,
	u32 *pdwMAC_MSBs,
	u32 *pdwMAC_LSBs
);
/*******************************
  Tone Detector control
*******************************/
u16 HPI_ToneDetector_GetState(
	struct hpi_hsubsys *phSubSys,
	u32 hC,
	u32 *State
);

u16 HPI_ToneDetector_SetEnable(
	struct hpi_hsubsys *phSubSys,
	u32 hC,
	u32 Enable
);

u16 HPI_ToneDetector_GetEnable(
	struct hpi_hsubsys *phSubSys,
	u32 hC,
	u32 *Enable
);

u16 HPI_ToneDetector_SetEventEnable(
	struct hpi_hsubsys *phSubSys,
	u32 hC,
	u32 EventEnable
);

u16 HPI_ToneDetector_GetEventEnable(
	struct hpi_hsubsys *phSubSys,
	u32 hC,
	u32 *EventEnable
);

u16 HPI_ToneDetector_SetThreshold(
	struct hpi_hsubsys *phSubSys,
	u32 hC,
	int Threshold
);

u16 HPI_ToneDetector_GetThreshold(
	struct hpi_hsubsys *phSubSys,
	u32 hC,
	int *Threshold
);

u16 HPI_ToneDetector_GetFrequency(
	struct hpi_hsubsys *phSubSys,
	u32 hC,
	u32 nIndex,
	u32 *dwFrequency
);

/*******************************
  Silence Detector control
*******************************/
u16 HPI_SilenceDetector_GetState(
	struct hpi_hsubsys *phSubSys,
	u32 hC,
	u32 *State
);

u16 HPI_SilenceDetector_SetEnable(
	struct hpi_hsubsys *phSubSys,
	u32 hC,
	u32 Enable
);

u16 HPI_SilenceDetector_GetEnable(
	struct hpi_hsubsys *phSubSys,
	u32 hC,
	u32 *Enable
);

u16 HPI_SilenceDetector_SetEventEnable(
	struct hpi_hsubsys
	*phSubSys,
	u32 hC,
	u32 EventEnable
);

u16 HPI_SilenceDetector_GetEventEnable(
	struct hpi_hsubsys
	*phSubSys,
	u32 hC,
	u32 *EventEnable
);

u16 HPI_SilenceDetector_SetDelay(
	struct hpi_hsubsys *phSubSys,
	u32 hC,
	u32 Delay
);

u16 HPI_SilenceDetector_GetDelay(
	struct hpi_hsubsys *phSubSys,
	u32 hC,
	u32 *Delay
);

u16 HPI_SilenceDetector_SetThreshold(
	struct hpi_hsubsys *phSubSys,
	u32 hC,
	int Threshold
);

u16 HPI_SilenceDetector_GetThreshold(
	struct hpi_hsubsys *phSubSys,
	u32 hC,
	int *Threshold
);

/*/////////// */
/* DSP CLOCK  */
/*/////////// */
u16 HPI_ClockOpen(
	struct hpi_hsubsys *phSubSys,
	u16 wAdapterIndex,
	u32 *phDspClock
);

u16 HPI_ClockSetTime(
	struct hpi_hsubsys *phSubSys,
	u32 hClock,
	u16 wHour,
	u16 wMinute,
	u16 wSecond,
	u16 wMilliSecond
);

u16 HPI_ClockGetTime(
	struct hpi_hsubsys *phSubSys,
	u32 hClock,
	u16 *pwHour,
	u16 *pwMinute,
	u16 *pwSecond,
	u16 *pwMilliSecond
);

/*/////////// */
/* PROFILE	  */
/*/////////// */
u16 HPI_ProfileOpenAll(
	struct hpi_hsubsys *phSubSys,
	u16 wAdapterIndex,
	u16 wProfileIndex,
	u32 *phProfile,
	u16 *pwMaxProfiles
);

u16 HPI_ProfileGet(
	struct hpi_hsubsys *phSubSys,
	u32 hProfile,
	u16 wIndex,
	u16 *pwSeconds,
	u32 *pdwMicroSeconds,
	u32 *pdwCallCount,
	u32 *pdwMaxMicroSeconds,
	u32 *pdwMinMicroSeconds
);

u16 HPI_ProfileStartAll(
	struct hpi_hsubsys *phSubSys,
	u32 hProfile
);

u16 HPI_ProfileStopAll(
	struct hpi_hsubsys *phSubSys,
	u32 hProfile
);

u16 HPI_ProfileGetName(
	struct hpi_hsubsys *phSubSys,
	u32 hProfile,
	u16 wIndex,
	char *szProfileName,
	u16 nProfileNameLength
);

u16 HPI_ProfileGetUtilization(
	struct hpi_hsubsys *phSubSys,
	u32 hProfile,
	u32 *pdwUtilization
);

/*//////////////////// */
/* UTILITY functions */

u16 HPI_FormatCreate(
	struct hpi_format *pFormat,
	u16 wChannels,
	u16 wFormat,
	u32 dwSampleRate,
	u32 dwBitRate,
	u32 dwAttributes
);

/* Until it's verified, this function is for Windows OSs only */

/*****************************************************************************/
/*****************************************************************************/
/********		HPI LOW LEVEL MESSAGES			*******/
/*****************************************************************************/
/*****************************************************************************/
/** Pnp ids */
/** "ASI"  - actual is "ASX" - need to change */
#define HPI_ID_ISAPNP_AUDIOSCIENCE	0x0669
/** PCI vendor ID that AudioScience uses */
#define HPI_PCI_VENDOR_ID_AUDIOSCIENCE	0x175C
/** PCI vendor ID that the DSP56301 has */
#define HPI_PCI_VENDOR_ID_MOTOROLA	0x1057
/** PCI vendor ID that TI uses */
#define HPI_PCI_VENDOR_ID_TI		0x104C

#define HPI_USB_VENDOR_ID_AUDIOSCIENCE	0x1257
#define HPI_USB_W2K_TAG			0x57495341	/* "ASIW"	*/
#define HPI_USB_LINUX_TAG		0x4C495341	/* "ASIL"	*/

/******************************************* message types */
#define HPI_TYPE_MESSAGE			1
#define HPI_TYPE_RESPONSE			2
#define HPI_TYPE_DATA				3
#define HPI_TYPE_SSX2BYPASS_MESSAGE		4

/******************************************* object types */
#define HPI_OBJ_SUBSYSTEM			1
#define HPI_OBJ_ADAPTER				2
#define HPI_OBJ_OSTREAM				3
#define HPI_OBJ_ISTREAM				4
#define HPI_OBJ_MIXER				5
#define HPI_OBJ_NODE				6
#define HPI_OBJ_CONTROL				7
#define HPI_OBJ_NVMEMORY			8
#define HPI_OBJ_GPIO				9
#define HPI_OBJ_WATCHDOG			10
#define HPI_OBJ_CLOCK				11
#define HPI_OBJ_PROFILE				12
#define HPI_OBJ_CONTROLEX			13
#define HPI_OBJ_ASYNCEVENT			14

#define HPI_OBJ_MAXINDEX			14

/******************************************* methods/functions */

#define HPI_OBJ_FUNCTION_SPACING	0x100
#define HPI_MAKE_INDEX(obj, index) (obj * HPI_OBJ_FUNCTION_SPACING + index)
#define HPI_EXTRACT_INDEX(fn) (fn & 0xff)

/* SUB-SYSTEM */
#define HPI_SUBSYS_OPEN			HPI_MAKE_INDEX(HPI_OBJ_SUBSYSTEM, 1)
#define HPI_SUBSYS_GET_VERSION		HPI_MAKE_INDEX(HPI_OBJ_SUBSYSTEM, 2)
#define HPI_SUBSYS_GET_INFO		HPI_MAKE_INDEX(HPI_OBJ_SUBSYSTEM, 3)
#define HPI_SUBSYS_FIND_ADAPTERS	HPI_MAKE_INDEX(HPI_OBJ_SUBSYSTEM, 4)
#define HPI_SUBSYS_CREATE_ADAPTER	HPI_MAKE_INDEX(HPI_OBJ_SUBSYSTEM, 5)
#define HPI_SUBSYS_CLOSE		HPI_MAKE_INDEX(HPI_OBJ_SUBSYSTEM, 6)
#define HPI_SUBSYS_DELETE_ADAPTER	HPI_MAKE_INDEX(HPI_OBJ_SUBSYSTEM, 7)
#define HPI_SUBSYS_DRIVER_LOAD		HPI_MAKE_INDEX(HPI_OBJ_SUBSYSTEM, 8)
#define HPI_SUBSYS_DRIVER_UNLOAD	HPI_MAKE_INDEX(HPI_OBJ_SUBSYSTEM, 9)
 /*SGT*/
#define HPI_SUBSYS_READ_PORT_8		HPI_MAKE_INDEX(HPI_OBJ_SUBSYSTEM, 10)
#define HPI_SUBSYS_WRITE_PORT_8		HPI_MAKE_INDEX(HPI_OBJ_SUBSYSTEM, 11)
#define HPI_SUBSYS_GET_NUM_ADAPTERS	HPI_MAKE_INDEX(HPI_OBJ_SUBSYSTEM, 12)
#define HPI_SUBSYS_GET_ADAPTER		HPI_MAKE_INDEX(HPI_OBJ_SUBSYSTEM, 13)
#define HPI_SUBSYS_SET_NETWORK_INTERFACE HPI_MAKE_INDEX(HPI_OBJ_SUBSYSTEM, 14)
/* ADAPTER */
#define HPI_ADAPTER_OPEN		HPI_MAKE_INDEX(HPI_OBJ_ADAPTER, 1)
#define HPI_ADAPTER_CLOSE		HPI_MAKE_INDEX(HPI_OBJ_ADAPTER, 2)
#define HPI_ADAPTER_GET_INFO		HPI_MAKE_INDEX(HPI_OBJ_ADAPTER, 3)
#define HPI_ADAPTER_GET_ASSERT		HPI_MAKE_INDEX(HPI_OBJ_ADAPTER, 4)
#define HPI_ADAPTER_TEST_ASSERT		HPI_MAKE_INDEX(HPI_OBJ_ADAPTER, 5)
#define HPI_ADAPTER_SET_MODE		HPI_MAKE_INDEX(HPI_OBJ_ADAPTER, 6)
#define HPI_ADAPTER_GET_MODE		HPI_MAKE_INDEX(HPI_OBJ_ADAPTER, 7)
#define HPI_ADAPTER_ENABLE_CAPABILITY	HPI_MAKE_INDEX(HPI_OBJ_ADAPTER, 8)
#define HPI_ADAPTER_SELFTEST		HPI_MAKE_INDEX(HPI_OBJ_ADAPTER, 9)
#define HPI_ADAPTER_FIND_OBJECT		HPI_MAKE_INDEX(HPI_OBJ_ADAPTER, 10)
#define HPI_ADAPTER_QUERY_FLASH		HPI_MAKE_INDEX(HPI_OBJ_ADAPTER, 11)
#define HPI_ADAPTER_START_FLASH		HPI_MAKE_INDEX(HPI_OBJ_ADAPTER, 12)
#define HPI_ADAPTER_PROGRAM_FLASH	HPI_MAKE_INDEX(HPI_OBJ_ADAPTER, 13)
#define HPI_ADAPTER_SET_PROPERTY	HPI_MAKE_INDEX(HPI_OBJ_ADAPTER, 14)
#define HPI_ADAPTER_GET_PROPERTY	HPI_MAKE_INDEX(HPI_OBJ_ADAPTER, 15)
#define HPI_ADAPTER_ENUM_PROPERTY	HPI_MAKE_INDEX(HPI_OBJ_ADAPTER, 16)
#define HPI_ADAPTER_MODULE_INFO		HPI_MAKE_INDEX(HPI_OBJ_ADAPTER, 17)
#define HPI_ADAPTER_FUNCTION_COUNT 17
/* OUTPUT STREAM */
#define HPI_OSTREAM_OPEN		HPI_MAKE_INDEX(HPI_OBJ_OSTREAM, 1)
#define HPI_OSTREAM_CLOSE		HPI_MAKE_INDEX(HPI_OBJ_OSTREAM, 2)
#define HPI_OSTREAM_WRITE		HPI_MAKE_INDEX(HPI_OBJ_OSTREAM, 3)
#define HPI_OSTREAM_START		HPI_MAKE_INDEX(HPI_OBJ_OSTREAM, 4)
#define HPI_OSTREAM_STOP		HPI_MAKE_INDEX(HPI_OBJ_OSTREAM, 5)
#define HPI_OSTREAM_RESET		HPI_MAKE_INDEX(HPI_OBJ_OSTREAM, 6)
#define HPI_OSTREAM_GET_INFO		HPI_MAKE_INDEX(HPI_OBJ_OSTREAM, 7)
#define HPI_OSTREAM_QUERY_FORMAT	HPI_MAKE_INDEX(HPI_OBJ_OSTREAM, 8)
#define HPI_OSTREAM_DATA		HPI_MAKE_INDEX(HPI_OBJ_OSTREAM, 9)
#define HPI_OSTREAM_SET_VELOCITY	HPI_MAKE_INDEX(HPI_OBJ_OSTREAM, 10)
#define HPI_OSTREAM_SET_PUNCHINOUT	HPI_MAKE_INDEX(HPI_OBJ_OSTREAM, 11)
#define HPI_OSTREAM_SINEGEN		HPI_MAKE_INDEX(HPI_OBJ_OSTREAM, 12)
#define HPI_OSTREAM_ANC_RESET		HPI_MAKE_INDEX(HPI_OBJ_OSTREAM, 13)
#define HPI_OSTREAM_ANC_GET_INFO	HPI_MAKE_INDEX(HPI_OBJ_OSTREAM, 14)
#define HPI_OSTREAM_ANC_READ		HPI_MAKE_INDEX(HPI_OBJ_OSTREAM, 15)
#define HPI_OSTREAM_SET_TIMESCALE	HPI_MAKE_INDEX(HPI_OBJ_OSTREAM, 16)
#define HPI_OSTREAM_SET_FORMAT		HPI_MAKE_INDEX(HPI_OBJ_OSTREAM, 17)
#define HPI_OSTREAM_HOSTBUFFER_ALLOC	HPI_MAKE_INDEX(HPI_OBJ_OSTREAM, 18)
#define HPI_OSTREAM_HOSTBUFFER_FREE	HPI_MAKE_INDEX(HPI_OBJ_OSTREAM, 19)
#define HPI_OSTREAM_GROUP_ADD		HPI_MAKE_INDEX(HPI_OBJ_OSTREAM, 20)
#define HPI_OSTREAM_GROUP_GETMAP	HPI_MAKE_INDEX(HPI_OBJ_OSTREAM, 21)
#define HPI_OSTREAM_GROUP_RESET		HPI_MAKE_INDEX(HPI_OBJ_OSTREAM, 22)
#define HPI_OSTREAM_FUNCTION_COUNT		(22)
/* INPUT STREAM */
#define HPI_ISTREAM_OPEN		HPI_MAKE_INDEX(HPI_OBJ_ISTREAM, 1)
#define HPI_ISTREAM_CLOSE		HPI_MAKE_INDEX(HPI_OBJ_ISTREAM, 2)
#define HPI_ISTREAM_SET_FORMAT		HPI_MAKE_INDEX(HPI_OBJ_ISTREAM, 3)
#define HPI_ISTREAM_READ		HPI_MAKE_INDEX(HPI_OBJ_ISTREAM, 4)
#define HPI_ISTREAM_START		HPI_MAKE_INDEX(HPI_OBJ_ISTREAM, 5)
#define HPI_ISTREAM_STOP		HPI_MAKE_INDEX(HPI_OBJ_ISTREAM, 6)
#define HPI_ISTREAM_RESET		HPI_MAKE_INDEX(HPI_OBJ_ISTREAM, 7)
#define HPI_ISTREAM_GET_INFO		HPI_MAKE_INDEX(HPI_OBJ_ISTREAM, 8)
#define HPI_ISTREAM_QUERY_FORMAT	HPI_MAKE_INDEX(HPI_OBJ_ISTREAM, 9)
#define HPI_ISTREAM_ANC_RESET		HPI_MAKE_INDEX(HPI_OBJ_ISTREAM, 10)
#define HPI_ISTREAM_ANC_GET_INFO	HPI_MAKE_INDEX(HPI_OBJ_ISTREAM, 11)
#define HPI_ISTREAM_ANC_WRITE		HPI_MAKE_INDEX(HPI_OBJ_ISTREAM, 12)
#define HPI_ISTREAM_HOSTBUFFER_ALLOC	HPI_MAKE_INDEX(HPI_OBJ_ISTREAM, 13)
#define HPI_ISTREAM_HOSTBUFFER_FREE	HPI_MAKE_INDEX(HPI_OBJ_ISTREAM, 14)
#define HPI_ISTREAM_GROUP_ADD		HPI_MAKE_INDEX(HPI_OBJ_ISTREAM, 15)
#define HPI_ISTREAM_GROUP_GETMAP	HPI_MAKE_INDEX(HPI_OBJ_ISTREAM, 16)
#define HPI_ISTREAM_GROUP_RESET		HPI_MAKE_INDEX(HPI_OBJ_ISTREAM, 17)
#define HPI_ISTREAM_FUNCTION_COUNT		(17)
/* MIXER */
/* NOTE:
   GET_NODE_INFO, SET_CONNECTION, GET_CONNECTIONS are not currently used */
#define HPI_MIXER_OPEN			HPI_MAKE_INDEX(HPI_OBJ_MIXER, 1)
#define HPI_MIXER_CLOSE			HPI_MAKE_INDEX(HPI_OBJ_MIXER, 2)
#define HPI_MIXER_GET_INFO		HPI_MAKE_INDEX(HPI_OBJ_MIXER, 3)
#define HPI_MIXER_GET_NODE_INFO		HPI_MAKE_INDEX(HPI_OBJ_MIXER, 4)
#define HPI_MIXER_GET_CONTROL		HPI_MAKE_INDEX(HPI_OBJ_MIXER, 5)
#define HPI_MIXER_SET_CONNECTION	HPI_MAKE_INDEX(HPI_OBJ_MIXER, 6)
#define HPI_MIXER_GET_CONNECTIONS	HPI_MAKE_INDEX(HPI_OBJ_MIXER, 7)
#define HPI_MIXER_GET_CONTROL_BY_INDEX	HPI_MAKE_INDEX(HPI_OBJ_MIXER, 8)
#define HPI_MIXER_GET_CONTROL_ARRAY_BY_INDEX  HPI_MAKE_INDEX(HPI_OBJ_MIXER, 9)
#define HPI_MIXER_GET_CONTROL_MULTIPLE_VALUES HPI_MAKE_INDEX(HPI_OBJ_MIXER, 10)
#define HPI_MIXER_STORE			HPI_MAKE_INDEX(HPI_OBJ_MIXER, 11)
#define HPI_MIXER_FUNCTION_COUNT	11
/* MIXER CONTROLS */
#define HPI_CONTROL_GET_INFO		HPI_MAKE_INDEX(HPI_OBJ_CONTROL, 1)
#define HPI_CONTROL_GET_STATE		HPI_MAKE_INDEX(HPI_OBJ_CONTROL, 2)
#define HPI_CONTROL_SET_STATE		HPI_MAKE_INDEX(HPI_OBJ_CONTROL, 3)
#define HPI_CONTROL_FUNCTION_COUNT 3
/* NONVOL MEMORY */
#define HPI_NVMEMORY_OPEN		HPI_MAKE_INDEX(HPI_OBJ_NVMEMORY, 1)
#define HPI_NVMEMORY_READ_BYTE		HPI_MAKE_INDEX(HPI_OBJ_NVMEMORY, 2)
#define HPI_NVMEMORY_WRITE_BYTE		HPI_MAKE_INDEX(HPI_OBJ_NVMEMORY, 3)
#define HPI_NVMEMORY_FUNCTION_COUNT 3
/* GPIO */
#define HPI_GPIO_OPEN			HPI_MAKE_INDEX(HPI_OBJ_GPIO, 1)
#define HPI_GPIO_READ_BIT		HPI_MAKE_INDEX(HPI_OBJ_GPIO, 2)
#define HPI_GPIO_WRITE_BIT		HPI_MAKE_INDEX(HPI_OBJ_GPIO, 3)
#define HPI_GPIO_READ_ALL		HPI_MAKE_INDEX(HPI_OBJ_GPIO, 4)
#define HPI_GPIO_FUNCTION_COUNT 4
/* ASYNC EVENT */
#define HPI_ASYNCEVENT_OPEN		HPI_MAKE_INDEX(HPI_OBJ_ASYNCEVENT, 1)
#define HPI_ASYNCEVENT_CLOSE		HPI_MAKE_INDEX(HPI_OBJ_ASYNCEVENT, 2)
#define HPI_ASYNCEVENT_WAIT		HPI_MAKE_INDEX(HPI_OBJ_ASYNCEVENT, 3)
#define HPI_ASYNCEVENT_GETCOUNT		HPI_MAKE_INDEX(HPI_OBJ_ASYNCEVENT, 4)
#define HPI_ASYNCEVENT_GET		HPI_MAKE_INDEX(HPI_OBJ_ASYNCEVENT, 5)
#define HPI_ASYNCEVENT_SENDEVENTS	HPI_MAKE_INDEX(HPI_OBJ_ASYNCEVENT, 6)
#define HPI_ASYNCEVENT_FUNCTION_COUNT 6
/* WATCH-DOG */
#define HPI_WATCHDOG_OPEN		HPI_MAKE_INDEX(HPI_OBJ_WATCHDOG, 1)
#define HPI_WATCHDOG_SET_TIME		HPI_MAKE_INDEX(HPI_OBJ_WATCHDOG, 2)
#define HPI_WATCHDOG_PING		HPI_MAKE_INDEX(HPI_OBJ_WATCHDOG, 3)
/* CLOCK */
#define HPI_CLOCK_OPEN			HPI_MAKE_INDEX(HPI_OBJ_CLOCK, 1)
#define HPI_CLOCK_SET_TIME		HPI_MAKE_INDEX(HPI_OBJ_CLOCK, 2)
#define HPI_CLOCK_GET_TIME		HPI_MAKE_INDEX(HPI_OBJ_CLOCK, 3)
/* PROFILE */
#define HPI_PROFILE_OPEN_ALL		HPI_MAKE_INDEX(HPI_OBJ_PROFILE, 1)
#define HPI_PROFILE_START_ALL		HPI_MAKE_INDEX(HPI_OBJ_PROFILE, 2)
#define HPI_PROFILE_STOP_ALL		HPI_MAKE_INDEX(HPI_OBJ_PROFILE, 3)
#define HPI_PROFILE_GET			HPI_MAKE_INDEX(HPI_OBJ_PROFILE, 4)
#define HPI_PROFILE_GET_IDLECOUNT	HPI_MAKE_INDEX(HPI_OBJ_PROFILE, 5)
#define HPI_PROFILE_GET_NAME		HPI_MAKE_INDEX(HPI_OBJ_PROFILE, 6)
#define HPI_PROFILE_GET_UTILIZATION	HPI_MAKE_INDEX(HPI_OBJ_PROFILE, 7)
#define HPI_PROFILE_FUNCTION_COUNT 7
/* ////////////////////////////////////////////////////////////////////// */
/* STRUCTURES */
#ifndef DISABLE_PRAGMA_PACK1
#pragma pack(push, 1)
#endif
/** PCI bus resource */
	struct hpi_pci {
	u32 __iomem *apMemBase[HPI_MAX_ADAPTER_MEM_SPACES];
	struct pci_dev *pOsData;

#ifndef HPI64BIT		/* keep structure size constant */
	u32 dwPadding[HPI_MAX_ADAPTER_MEM_SPACES + 1];
#endif
	u16 wVendorId;
	u16 wDeviceId;
	u16 wSubSysVendorId;
	u16 wSubSysDeviceId;
	u16 wBusNumber;
	u16 wDeviceNumber;
	u32 wInterrupt;
};

struct hpi_resource {
	union {
		struct hpi_pci *Pci;
		char *net_if;
	} r;
#ifndef HPI64BIT		/* keep structure size constant */
	u32 dwPadTo64;
#endif
	u16 wBusType;		/* HPI_BUS_PNPISA, _PCI, _USB etc */
	u16 wPadding;

};

/** Format info used inside struct hpi_message 
    Not the same as public API struct hpi_format */
struct hpi_msg_format {
	u32 dwSampleRate;
				/**< 11025, 32000, 44100 ... */
	u32 dwBitRate;	       /**< for MPEG */
	u32 dwAttributes;
				/**< Stereo/JointStereo/Mono */
	u16 wChannels;	       /**< 1,2..., (or ancillary mode or idle bit */
	u16 wFormat; /**< HPI_FORMAT_PCM16, _MPEG etc. see \ref formats. */
};

/**  Buffer+format structure.
	 Must be kept 7 * 32 bits to match public struct hpi_data HPI_DATAstruct */
struct hpi_msg_data {
	struct hpi_msg_format Format;
	u8 *pbData;
#ifndef HPI64BIT
	u32 dwPadding;
#endif
	u32 dwDataSize;
};

/** struct hpi_data HPI_DATAstructure used up to 3.04 driver */
struct hpi_data_legacy32 {
	struct hpi_format Format;
	u32 pbData;
	u32 dwDataSize;
};

#ifdef HPI64BIT
/* Compatibility version of struct hpi_data HPI_DATA*/
struct hpi_data_compat32 {
	struct hpi_msg_format Format;
	u32 pbData;
	u32 dwPadding;
	u32 dwDataSize;
};
#endif

struct hpi_buffer {
  /** placehoder for backward compatability (see dwBufferSize) */
	struct hpi_msg_format reserved;
	u32 dwCommand;	  /**< HPI_BUFFER_CMD_xxx*/
	u32 dwPciAddress; /**< PCI physical address of buffer for DSP DMA */
	u32 dwBufferSize; /**< must line up with dwDataSize of HPI_DATA*/
};

struct hpi_subsys_msg {
	struct hpi_resource Resource;
};

struct hpi_subsys_res {
	u32 dwVersion;
	u32 dwData;		/* used to return extended version */
	u16 wNumAdapters;	/* number of adapters */
	u16 wAdapterIndex;
	u16 awAdapterList[HPI_MAX_ADAPTERS];
};

struct hpi_adapter_msg {
	u32 dwAdapterMode;	/* adapter mode */
	u16 wAssertId;		/* assert number for "test assert" call
				   wObjectIndex for find object call
				   wQueryOrSet for HPI_AdapterSetModeEx() */
	u16 wObjectType;	/* for adapter find object call */
};

union hpi_adapterx_msg {
	struct hpi_adapter_msg adapter;
	struct {
		u32 dwOffset;
	} query_flash;
	struct {
		u32 dwOffset;
		u32 dwLength;
		u32 dwKey;
	} start_flash;
	struct {
		u32 dwChecksum;
		u16 wSequence;
		u16 wLength;
	} program_flash;
	struct {
		u16 wProperty;
		u16 wParameter1;
		u16 wParameter2;
	} property_set;
	struct {
		u16 wIndex;
		u16 wWhat;
		u16 wPropertyIndex;
	} property_enum;
	struct {
		u16 index;
	} module_info;
};

struct hpi_adapter_res {
	u32 dwSerialNumber;
	u16 wAdapterType;
	u16 wAdapterIndex;	/* Is this needed? also used for wDspIndex */
	u16 wNumIStreams;
	u16 wNumOStreams;
	u16 wNumMixers;
	u16 wVersion;
	u8 szAdapterAssert[STR_SIZE(HPI_STRING_LEN)];
};

union hpi_adapterx_res {
	struct hpi_adapter_res adapter;
	struct {
		u32 dwChecksum;
		u32 dwLength;
		u32 dwVersion;
	} query_flash;
	struct {
		u16 wSequence;
	} program_flash;
	struct {
		u16 wParameter1;
		u16 wParameter2;
	} property_get;
};

struct hpi_stream_msg {
	union {
		struct hpi_msg_data Data;
		struct hpi_data_legacy32 Data32;
		u16 wVelocity;
		struct hpi_punchinout Pio;
		u32 dwTimeScale;
		struct hpi_buffer Buffer;
		struct hpi_streamid Stream;
	} u;
	u16 wStreamIndex;
	u16 wIStreamIndex;
};

struct hpi_stream_res {
	union {
		struct {
			/* size of hardware buffer */
			u32 dwBufferSize;
			/* OutStream - data to play,
			   InStream - data recorded */
			u32 dwDataAvailable;
			/* OutStream - samples played,
			   InStream - samples recorded */
			u32 dwSamplesTransferred;
			/* Adapter - OutStream - data to play,
			   InStream - data recorded */
			u32 dwAuxiliaryDataAvailable;
			u16 wState;	/* HPI_STATE_PLAYING, _STATE_STOPPED */
			u16 wPadding;
		} stream_info;
		struct {
			u32 dwBufferSize;
			u32 dwDataAvailable;
			u32 dwSamplesTransfered;
			u16 wState;
			u16 wOStreamIndex;
			u16 wIStreamIndex;
			u16 wPadding;
			u32 dwAuxiliaryDataAvailable;
		} legacy_stream_info;
		struct {
			/* bitmap of grouped OutStreams */
			u32 dwOutStreamGroupMap;
			/* bitmap of grouped InStreams */
			u32 dwInStreamGroupMap;
		} group_info;
	} u;
};

struct hpi_mixer_msg {
	u16 wControlIndex;
	u16 wControlType;	/* = HPI_CONTROL_METER _VOLUME etc */
	u16 wPadding1;		/* Maintain alignment of subsequent fields */
	u16 wNodeType1;		/* = HPI_SOURCENODE_LINEIN etc */
	u16 wNodeIndex1;	/* = 0..N */
	u16 wNodeType2;
	u16 wNodeIndex2;
	u16 wPadding2;		/* round to 4 bytes */
};

struct hpi_mixer_res {
	u16 wSrcNodeType;	/* = HPI_SOURCENODE_LINEIN etc */
	u16 wSrcNodeIndex;	/* = 0..N */
	u16 wDstNodeType;
	u16 wDstNodeIndex;
	/* Also controlType for MixerGetControlByIndex */
	u16 wControlIndex;
	/* may indicate which DSP the control is located on */
	u16 wDspIndex;
};

union hpi_mixerx_msg {
	struct {
		u16 wStartingIndex;
		u16 wFlags;
		u32 dwLengthInBytes;	/* length in bytes of pData */
		u32 pData;	/* pointer to a data array */
	} gcabi;
	struct {
		u16 wCommand;
		u16 wIndex;
	} store;		/* for HPI_MIXER_STORE message */
};

union hpi_mixerx_res {
	struct {
		u32 dwBytesReturned;	/* number of items returned */
		u32 pData;	/* pointer to data array */
		u16 wMoreToDo;	/* indicates if there is more to do */
	} gcabi;
};

struct hpi_control_msg {
	u32 dwParam1;		/* generic parameter 1 */
	u32 dwParam2;		/* generic parameter 2 */
	short anLogValue[HPI_MAX_CHANNELS];
	u16 wAttribute;		/* control attribute or property */
	u16 wControlIndex;
};

struct hpi_control_union_msg {
	union {
		struct {
			u32 dwParam1;	/* generic parameter 1 */
			u32 dwParam2;	/* generic parameter 2 */
			short anLogValue[HPI_MAX_CHANNELS];
		} old;
		union {
			u32 dwFrequency;
			u32 dwGain;
			u32 dwBand;
			struct {
				u32 dwMode;
				u32 dwValue;
			} mode;
		} tuner;
	} u;
	u16 wAttribute;		/* control attribute or property */
	u16 wControlIndex;
};

struct hpi_control_res {
	/* Could make union. dwParam, anLogValue never used in same response */
	u32 dwParam1;
	u32 dwParam2;
	short anLogValue[HPI_MAX_CHANNELS];
};

union hpi_control_union_res {
	struct {
		u32 dwParam1;
		u32 dwParam2;
		short anLogValue[HPI_MAX_CHANNELS];
	} old;
	union {
		u32 dwBand;
		u32 dwFrequency;
		u32 dwGain;
		u32 dwLevel;
		struct {
			u32 dwData[2];
			u32 dwBLER;
		} rds;
	} tuner;
};

/* HPI_CONTROLX_STRUCTURES */

/* Message */

/** Used for all HMI variables where max length <= 8 bytes
*/
struct hpi_controlx_msg_cobranet_data {
	u32 dwHmiAddress;
	u32 dwByteCount;
	u32 dwData[2];
};

/** Used for string data, and for packet bridge
*/
struct hpi_controlx_msg_cobranet_bigdata {
	u32 dwHmiAddress;
	u32 dwByteCount;
	u8 *pbData;
#ifndef HPI64BIT
	u32 dwPadding;
#endif
};

/** Used for generic data
*/

struct hpi_controlx_msg_generic {
	u32 dwParam1;
	u32 dwParam2;
};

struct hpi_controlx_msg {
	union {
		struct hpi_controlx_msg_cobranet_data cobranet_data;
		struct hpi_controlx_msg_cobranet_bigdata cobranet_bigdata;
		struct hpi_controlx_msg_generic generic;
		/* nothing extra to send for status read */
	} u;
	u16 wControlIndex;
	u16 wAttribute;		/* control attribute or property */
};

/* Response */

/**
*/
struct hpi_controlx_res_cobranet_data {
	u32 dwByteCount;
	u32 dwData[2];
};

struct hpi_controlx_res_cobranet_bigdata {
	u32 dwByteCount;
};

struct hpi_controlx_res_cobranet_status {
	u32 dwStatus;
	u32 dwReadableSize;
	u32 dwWriteableSize;
};

struct hpi_controlx_res_generic {
	u32 dwParam1;
	u32 dwParam2;
};

struct hpi_controlx_res {
	union {
		struct hpi_controlx_res_cobranet_bigdata cobranet_bigdata;
		struct hpi_controlx_res_cobranet_data cobranet_data;
		struct hpi_controlx_res_cobranet_status cobranet_status;
		struct hpi_controlx_res_generic generic;
	} u;
};

struct hpi_nvmemory_msg {
	u16 wIndex;
	u16 wData;
};

struct hpi_nvmemory_res {
	u16 wSizeInBytes;
	u16 wData;
};

struct hpi_gpio_msg {
	u16 wBitIndex;
	u16 wBitData;
};

struct hpi_gpio_res {
	u16 wNumberInputBits;
	u16 wNumberOutputBits;
	u16 wBitData;
	u16 wPadding;
};

struct hpi_async_msg {
	u32 dwEvents;
	u16 wMaximumEvents;
	u16 wPadding;
};

struct hpi_async_res {
	union {
		struct {
			u16 wCount;
		} count;
		struct {
			u32 dwEvents;
			u16 wNumberReturned;
			u16 wPadding;
		} get;
		struct hpi_async_event event;
	} u;
};

struct hpi_watchdog_msg {
	u32 dwTimeMs;
};

struct hpi_watchdog_res {
	u32 dwTimeMs;
};

struct hpi_clock_msg {
	u16 wHours;
	u16 wMinutes;
	u16 wSeconds;
	u16 wMilliSeconds;
};

struct hpi_clock_res {
	u16 wSizeInBytes;
	u16 wHours;
	u16 wMinutes;
	u16 wSeconds;
	u16 wMilliSeconds;
	u16 wPadding;
};

struct hpi_profile_msg {
	u16 wIndex;
	u16 wPadding;
};

struct hpi_profile_res_open {
	u16 wMaxProfiles;
};

struct hpi_profile_res_time {
	u32 dwMicroSeconds;
	u32 dwCallCount;
	u32 dwMaxMicroSeconds;
	u32 dwMinMicroSeconds;
	u16 wSeconds;
};

struct hpi_profile_res_name {
/* u8 messes up response size for 56301 DSP */
	u16 szName[16];
};

struct hpi_profile_res {
	union {
		struct hpi_profile_res_open o;
		struct hpi_profile_res_time t;
		struct hpi_profile_res_name n;
	} u;
};

struct hpi_message_header {
	u16 wSize;
	u16 wType;		/* HPI_MSG_MESSAGE, HPI_MSG_RESPONSE */
	u16 wObject;		/* HPI_OBJ_* */
	u16 wFunction;		/* HPI_SUBSYS_xxx, HPI_ADAPTER_xxx */
	u16 wAdapterIndex;	/* the adapter index */
	u16 wDspIndex;		/* the dsp index on the adapter */
};

struct hpi_message {
	/* following fields must match HPI_MESSAGE_HEADER */
	u16 wSize;
	u16 wType;		/* HPI_TYPE_MESSAGE, HPI_TYPE_RESPONSE */
	u16 wObject;		/* HPI_OBJ_* */
	u16 wFunction;		/* HPI_SUBSYS_xxx, HPI_ADAPTER_xxx */
	u16 wAdapterIndex;	/* the adapter index */
	u16 wDspIndex;		/* the dsp index on the adapter */
	union {
		struct hpi_subsys_msg s;
		struct hpi_adapter_msg a;
		union hpi_adapterx_msg ax;
		struct hpi_stream_msg d;
		struct hpi_mixer_msg m;
		union hpi_mixerx_msg mx;	/* extended mixer; */
		struct hpi_control_msg c;	/* mixer control; */
		/* identical to struct hpi_control_msg,
		   but field naming is improved */
		struct hpi_control_union_msg cu;
		struct hpi_controlx_msg cx;	/* extended mixer control; */
		struct hpi_nvmemory_msg n;
		struct hpi_gpio_msg l;	/* digital i/o */
		struct hpi_watchdog_msg w;
		struct hpi_clock_msg t;	/* dsp time */
		struct hpi_profile_msg p;
		struct hpi_async_msg as;
	} u;
};

#define HPI_MESSAGE_SIZE_BY_OBJECT { \
	sizeof(struct hpi_message_header) ,   /* Default, no object type 0 */ \
	sizeof(struct hpi_message_header) + sizeof(struct hpi_subsys_msg),\
	sizeof(struct hpi_message_header) + sizeof(union hpi_adapterx_msg),\
	sizeof(struct hpi_message_header) + sizeof(struct hpi_stream_msg),\
	sizeof(struct hpi_message_header) + sizeof(struct hpi_stream_msg),\
	sizeof(struct hpi_message_header) + sizeof(struct hpi_mixer_msg),\
	sizeof(struct hpi_message_header) ,   /* no node message */ \
	sizeof(struct hpi_message_header) + sizeof(struct hpi_control_msg),\
	sizeof(struct hpi_message_header) + sizeof(struct hpi_nvmemory_msg),\
	sizeof(struct hpi_message_header) + sizeof(struct hpi_gpio_msg),\
	sizeof(struct hpi_message_header) + sizeof(struct hpi_watchdog_msg),\
	sizeof(struct hpi_message_header) + sizeof(struct hpi_clock_msg),\
	sizeof(struct hpi_message_header) + sizeof(struct hpi_profile_msg),\
	sizeof(struct hpi_message_header) + sizeof(struct hpi_controlx_msg),\
	sizeof(struct hpi_message_header) + sizeof(struct hpi_async_msg) \
}

struct hpi_response_header {
	u16 wSize;
	u16 wType;		/* HPI_MSG_MESSAGE, HPI_MSG_RESPONSE */
	u16 wObject;		/* HPI_OBJ_* */
	u16 wFunction;		/* HPI_SUBSYS_xxx, HPI_ADAPTER_xxx */
	u16 wError;		/* HPI_ERROR_xxx */
	u16 wSpecificError;	/* Adapter specific error */
};

struct hpi_response {
/* following fields must match HPI_RESPONSE_HEADER */
	u16 wSize;
	u16 wType;		/* HPI_MSG_MESSAGE, HPI_MSG_RESPONSE */
	u16 wObject;		/* HPI_OBJ_* */
	u16 wFunction;		/* HPI_SUBSYS_xxx, HPI_ADAPTER_xxx */
	u16 wError;		/* HPI_ERROR_xxx */
	u16 wSpecificError;	/* Adapter specific error */
	union {
		struct hpi_subsys_res s;
		struct hpi_adapter_res a;
		union hpi_adapterx_res ax;
		struct hpi_stream_res d;
		struct hpi_mixer_res m;
		union hpi_mixerx_res mx;	/* extended mixer; */
		struct hpi_control_res c;	/* mixer control; */
		/* identical to hpi_control_res, but field naming is improved */
		union hpi_control_union_res cu;
		struct hpi_controlx_res cx;	/* extended mixer control; */
		struct hpi_nvmemory_res n;
		struct hpi_gpio_res l;	/* digital i/o */
		struct hpi_watchdog_res w;
		struct hpi_clock_res t;	/* dsp time */
		struct hpi_profile_res p;
		struct hpi_async_res as;
	} u;
};

#define HPI_RESPONSE_SIZE_BY_OBJECT { \
	sizeof(struct hpi_response_header) ,/* Default, no object type 0 */ \
	sizeof(struct hpi_response_header) + sizeof(struct hpi_subsys_res),\
	sizeof(struct hpi_response_header) + sizeof(union  hpi_adapterx_res),\
	sizeof(struct hpi_response_header) + sizeof(struct hpi_stream_res),\
	sizeof(struct hpi_response_header) + sizeof(struct hpi_stream_res),\
	sizeof(struct hpi_response_header) + sizeof(struct hpi_mixer_res),\
	sizeof(struct hpi_response_header) , /* no node response */ \
	sizeof(struct hpi_response_header) + sizeof(struct hpi_control_res),\
	sizeof(struct hpi_response_header) + sizeof(struct hpi_nvmemory_res),\
	sizeof(struct hpi_response_header) + sizeof(struct hpi_gpio_res),\
	sizeof(struct hpi_response_header) + sizeof(struct hpi_watchdog_res),\
	sizeof(struct hpi_response_header) + sizeof(struct hpi_clock_res),\
	sizeof(struct hpi_response_header) + sizeof(struct hpi_profile_res),\
	sizeof(struct hpi_response_header) + sizeof(struct hpi_controlx_res),\
	sizeof(struct hpi_response_header) + sizeof(struct hpi_async_res) \
}

/*////////////////////////////////////////////////////////////////////////// */
/* declarations for compact control calls  */
struct hpi_control_defn {
	u8 wType;
	u8 wChannels;
	u8 wSrcNodeType;
	u8 wSrcNodeIndex;
	u8 wDestNodeType;
	u8 wDestNodeIndex;
};

/*////////////////////////////////////////////////////////////////////////// */
/* declarations for control caching (internal to HPI<->DSP interaction)      */

/** A compact representation of (part of) a controls state.
Used for efficient transfer of the control state
between DSP and host or across a network
*/
struct hpi_control_cache_single {
	/** one of HPI_CONTROL_* */
	u16 ControlType;
	/** The original index of the control on the DSP */
	u16 ControlIndex;
	union {
		struct {	/* volume */
			u16 anLog[2];
		} v;
		struct {	/* peak meter */
			u16 anLogPeak[2];
			u16 anLogRMS[2];
		} p;
		struct {	/* channel mode */
			u16 wMode;
		} m;
		struct {	/* multiplexer */
			u16 wSourceNodeType;
			u16 wSourceNodeIndex;
		} x;
		struct {	/* level/trim */
			u16 anLog[2];
		} l;
		struct {	/* tuner - partial caching.
				   Some attributes go to the DSP. */
			u32 dwFreqInkHz;
			u16 wBand;
			u16 wLevel;
		} t;
		struct {	/* AESEBU Rx status */
			u32 dwErrorStatus;
			u32 dwSource;
		} aes3rx;
		struct {	/* AESEBU Tx */
			u32 dwFormat;
		} aes3tx;
		struct {	/* tone detector */
			u16 wState;
		} tone;
		struct {	/* silence detector */
			u32 dwState;
			u32 dwCount;
		} silence;
		struct {	/* sample clock */
			u16 wSource;
			u16 wSourceIndex;
			u32 dwSampleRate;
		} clk;
		struct {	/* generic control */
			u32 dw1;
			u32 dw2;
		} g;
	} u;
};
/*/////////////////////////////////////////////////////////////////////////// */
/* declarations for 2^N sized FIFO buffer (internal to HPI<->DSP interaction) */
struct hpi_fifo_buffer {
	u32 dwSize;
	u32 dwDSPIndex;
	u32 dwHostIndex;
};

#ifndef DISABLE_PRAGMA_PACK1
#pragma pack(pop)
#endif

/* skip host side function declarations for DSP
   compile and documentation extraction */
void HPI_InitMessage(
	struct hpi_message *phm,
	u16 wObject,
	u16 wFunction
);

void HPI_InitResponse(
	struct hpi_response *phr,
	u16 wObject,
	u16 wFunction,
	u16 wError
);

char HPI_HandleObject(
	const u32 dwHandle
);

void HPI_HandleToIndexes(
	const u32 dwHandle,
	u16 *pwAdapterIndex,
	u16 *pwObjectIndex,
	u16 *pwDspIndex
);

u32 HPI_IndexesToHandle(
	const char cObject,
	const u16 wAdapterIndex,
	const u16 wObjectIndex,
	const u16 wDspIndex
);

/*////////////////////////////////////////////////////////////////////////// */

/* main HPI entry point */
HPI_HandlerFunc HPI_Message;

/* UDP message */
void HPI_MessageUDP(
	struct hpi_message *phm,
	struct hpi_response *phr,
	unsigned int nTimeout
);

/* used in PnP OS/driver */
u16 HPI_SubSysCreateAdapter(
	struct hpi_hsubsys *phSubSys,
	struct hpi_resource *pResource,
	u16 *pwAdapterIndex
);

u16 HPI_SubSysDeleteAdapter(
	struct hpi_hsubsys *phSubSys,
	u16 wAdapterIndex
);

void HPI_FormatToMsg(
	struct hpi_msg_format *pMF,
	struct hpi_format *pF
);
void HPI_StreamResponseToLegacy(
	struct hpi_stream_res *pSR
);

/*////////////////////////////////////////////////////////////////////////// */
/* declarations for individual HPI entry points */
HPI_HandlerFunc HPI_4000;
HPI_HandlerFunc HPI_6000;
HPI_HandlerFunc HPI_6205;
HPI_HandlerFunc HPI_COMMON;

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif
#endif	 /*_H_HPI_ */
/*
///////////////////////////////////////////////////////////////////////////////
// See CVS for history.  Last complete set in rev 1.146
////////////////////////////////////////////////////////////////////////////////
*/
