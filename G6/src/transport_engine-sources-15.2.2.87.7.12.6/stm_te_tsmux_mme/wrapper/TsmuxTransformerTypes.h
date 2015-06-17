/************************************************************************
Copyright (C) 2012 STMicroelectronics. All Rights Reserved.

This is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

This is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

This may alternatively be licensed under a proprietary
license from ST.

Source file name : TsmuxTransformerTypes.h
Architects :	   Dan Thomson & Pete Steiglitz

Definition of the multiplexor mme transformer types  derived from MPEG2
header file


Date	Modification				Architects
----	------------				----------
26-Mar-12   Created				DT & PS

************************************************************************/

#ifndef TSMUX_TRANSFORMER_TYPES_H__
#define TSMUX_TRANSFORMER_TYPES_H__

#define TSMUX_MME_TRANSFORMER_NAME    "TSMUX_TRANSFORMER"

/*
 TSMUX_MME_VERSION:
 Identifies the MME API version of the MULTIPLEXOR firmware.
 If wants to build the firmware for old MME API version, change this string
 correspondingly.
 */

#ifndef TSMUX_MME_VERSION
/*!< Latest MME API version */
#define TSMUX_MME_VERSION	10
#endif

#if (TSMUX_MME_VERSION == 10)
#define TSMUX_MME_API_VERSION "1.0"
#endif

/* Chosen at random for now */
#define TSMUX_ID			0xDEAD
#define TSMUX_BASE		(TSMUX_ID << 16)

/* Used for array sizing, actuals may be restricted by capabilities */
#define TSMUX_MAX_PROGRAMS			4
#define TSMUX_MAX_STREAMS_PER_PROGRAM		7
#define TSMUX_MAX_BUFFERS			512

/* Must be divisible by 8 */
#define TSMUX_MAXIMUM_PROGRAM_NAME_SIZE	16
/* Must be divisible by 8 */
#define TSMUX_MAXIMUM_DESCRIPTOR_SIZE		16
/* Must be divisible by 8 */
#define TSMUX_MAXIMUM_INDEX_IDENTIFIER_SIZE	8
#define TSMUX_MME_UNSPECIFIED			0xffffffffU

/*typedef MME_GENERIC64		TSMUX_MME_TIMESTAMP;*/
typedef uint64_t TSMUX_MME_TIMESTAMP;

/* --- TSMUX_TransformerCapability_t --- */

typedef struct {
	/* Size of the structure in bytes */
	MME_UINT StructSize;
	/* TSMUX_MME_API_VERSION 1.0 => 10 */
	MME_UINT ApiVersion;
	/* The maximum number of programs that can be supported in this
	 * implementation. */
	MME_UINT MaximumNumberOfPrograms;
	/* The maximum number of streams that can be supported in this
	 * implementation. */
	MME_UINT MaximumNumberOfStreams;
	/* The maximum number of buffers that can be queued to a stream in this
	 * implementation */
	MME_UINT MaximumNumberOfBuffersPerStream;

} TSMUX_TransformerCapability_t;

/* --- TSMUX_InitTransformerParam_t --- */

#define TSMUX_TABLE_GENERATION_NONE				0
#define TSMUX_TABLE_GENERATION_PAT_PMT				1
#define TSMUX_TABLE_GENERATION_SDT				4

typedef struct {
	/* Size of the structure in bytes */
	MME_UINT StructSize;

	/* if non-zero => 192 byte packets */
	MME_UINT TimeStampedPackets;
	/* Specify the PCR period */
	MME_UINT PcrPeriod90KhzTicks;
	/* if non-zero => PCRs to be output on a non-data pid */
	MME_UINT GeneratePcrStream;
	/* Pid on which to output PCRs */
	MME_UINT PcrPid;
	/* Specify what tables to generate */
	MME_UINT TableGenerationFlags;
	/* Specify how often to output tables (minimum value) */
	MME_UINT TablePeriod90KhzTicks;
	/* PAT field, only relevent for pat/pmt generation */
	MME_UINT TransportStreamId;
	/* Force bitrate to fixed value */
	MME_UINT FixedBitrate;
	/* Pealk if not Fixed (bps) */
	MME_UINT Bitrate;
	/* Force transform to record index records */
	MME_UINT RecordIndexIdentifiers;
	/* To set discontinuity indicator in TS packet
	 * in case of full time discontinuity */
	MME_UINT GlobalFlags;
} TSMUX_InitTransformerParam_t;

/* --- TSMUX_AddProgram_t --- */

typedef struct {
	/* 0..(MaximumNumberOfPrograms-1) */
	MME_UINT ProgramId;
	/* Flag indicating this program is used only for tables, will not
	 * appear in any PAT/PMT */
	MME_UINT TablesProgram;
	/* PAT field value -- NOTE 0 should only be used for NIT table */
	MME_UINT ProgramNumber;
	/* Used Only for PMT */
	MME_UINT PMTPid;
	/*Used Only for simple SDT */
	MME_UINT StreamsMayBeScrambled;
	/* Specify name for generated SDT table. */
	char ProviderName[TSMUX_MAXIMUM_PROGRAM_NAME_SIZE];
	/* Specify name for generated SDT table. */
	char ServiceName[TSMUX_MAXIMUM_PROGRAM_NAME_SIZE];
	/* 0=>no descriptor, only used if generating tables */
	MME_UINT OptionalDescriptorSize;
	unsigned char Descriptor[TSMUX_MAXIMUM_DESCRIPTOR_SIZE];
} TSMUX_AddProgram_t;

/* --- TSMUX_RemoveProgram_t --- */

typedef struct {
	MME_UINT ProgramId;
} TSMUX_RemoveProgram_t;

/* --- TSMUX_AddTSMUX_STREAM_t --- */

#define TSMUX_STREAM_CONTENT_PES		1
#define TSMUX_STREAM_CONTENT_SECTION		2

/* */

#define TSMUX_STREAM_TYPE_RESERVED		0x00
#define TSMUX_STREAM_TYPE_VIDEO_MPEG1		0x01
#define TSMUX_STREAM_TYPE_VIDEO_MPEG2		0x02
#define TSMUX_STREAM_TYPE_AUDIO_MPEG1		0x03
#define TSMUX_STREAM_TYPE_AUDIO_MPEG2		0x04
#define TSMUX_STREAM_TYPE_PRIVATE_SECTIONS	0x05
#define TSMUX_STREAM_TYPE_PRIVATE_DATA		0x06
#define TSMUX_STREAM_TYPE_MHEG			0x07
#define TSMUX_STREAM_TYPE_DSMCC			0x08
#define TSMUX_STREAM_TYPE_H222_1		0x09

/* later extensions */
#define TSMUX_STREAM_TYPE_AUDIO_AAC		0x0f
#define TSMUX_STREAM_TYPE_VIDEO_MPEG4		0x10
#define TSMUX_STREAM_TYPE_VIDEO_H264		0x1b

/* private stream types */
#define TSMUX_STREAM_TYPE_PS_AUDIO_AC3		0x81
#define TSMUX_STREAM_TYPE_PS_AUDIO_DTS		0x8a
#define TSMUX_STREAM_TYPE_PS_AUDIO_LPCM		0x8b
#define TSMUX_STREAM_TYPE_PS_DVD_SUBPICTURE	0xff

/* Non-standard definitions */
#define TSMUX_STREAM_TYPE_VIDEO_DIRAC		0xD1

typedef struct {
	MME_UINT ProgramId;
	/* 0..(MaximumNumberOfStreams-1) */
	MME_UINT StreamId;
	/* Non-zero => output PCR packet onto this stream */
	MME_UINT IncorporatePcr;
	/* Pes/Section (one of TSMUX_STREAM_CONTENT_...) */
	MME_UINT StreamContent;
	/* Unspecified => 0x100 * (ProgramId+1) + StreamId */
	MME_UINT StreamPid;
	/* Used in PAT/PMT/SDT generation (one of TSMUX_STREAM_TYPE_...) */
	MME_UINT StreamType;
	/* 0=>no descriptor, only used if generating tables */
	MME_UINT OptionalDescriptorSize;
	unsigned char Descriptor[TSMUX_MAXIMUM_DESCRIPTOR_SIZE];
	/* Allow checks on DTS integrity */
	MME_UINT DTSIntegrityThreshold90KhzTicks;
	/* Major control on muxing (T-STD buffer) */
	MME_UINT DecoderBitBufferSize;
	/* Major control on muxing limit on transmitting data ahead */
	MME_UINT MultiplexAheadLimit90KhzTicks;
} TSMUX_AddStream_t;

/* --- TSMUX_StreamProgram_t --- */

typedef struct {
	MME_UINT ProgramId;
	MME_UINT StreamId;
} TSMUX_RemoveStream_t;

/* --- TSMUX_SetGlobalParams_t --- */

#define TSMUX_COMMAND_ADD_PROGRAM	1
#define TSMUX_COMMAND_REMOVE_PROGRAM	2
#define TSMUX_COMMAND_ADD_STREAM	3
#define TSMUX_COMMAND_REMOVE_STREAM	4

typedef struct {
	/* Size of the structure in bytes */
	MME_UINT StructSize;

	MME_UINT CommandCode;
	union {
		TSMUX_AddProgram_t AddProgram;
		TSMUX_RemoveProgram_t RemoveProgram;
		TSMUX_AddStream_t AddStream;
		TSMUX_RemoveStream_t RemoveStream;
	} CommandSubStructure;
} TSMUX_SetGlobalParams_t;

/* --- TSMUX_SendBuffersParam_t --- */
/* Used to fill time holes - DTS/Duration irrelevent */
#define TSMUX_BUFFER_FLAG_NO_DATA			1
#define TSMUX_BUFFER_FLAG_DISCONTINUITY			2
#define TSMUX_BUFFER_FLAG_TRANSPORT_SCRAMBLED		4
/* Only relevent if scrambled */
#define TSMUX_BUFFER_FLAG_KEY_PARITY			8
#define TSMUX_BUFFER_FLAG_REPEATING			16
/* Forces insertion of a DIT section in the stream before this buffer starts */
#define TSMUX_BUFFER_FLAG_REQUEST_DIT_INSERTION		32

/* Forces insertion of a RAP in the stream when pcr output */
#define TSMUX_BUFFER_FLAG_REQUEST_RAP_BIT		64

typedef struct {
	/* Size of the structure in bytes */
	MME_UINT StructSize;
	/* Program/stream identifier that buffers relate to */
	MME_UINT ProgramId;
	MME_UINT StreamId;
	/* DTS and duration */
	TSMUX_MME_TIMESTAMP DTS;
	/* NOTE for a multi packet section, duration is the interval
	 * over which it will be muxed into the stream.
	 * Repeat interval, is the period a section holds valid for. */
	MME_UINT DTSDuration;

	MME_UINT BufferFlags;
	/* Only for sections streams */
	MME_UINT RepeatInterval90KhzTicks;
	/* Used when a DIT is to be inserted. */
	MME_UINT DITTransitionFlagValue;
	/* Only when collecting indices */
	MME_UINT IndexIdentifierSize;
	unsigned char IndexIdentifier[TSMUX_MAXIMUM_INDEX_IDENTIFIER_SIZE];

} TSMUX_SendBuffersParam_t;

/* --- TSMUX_TransformParam_t --- */

/* Used before shutdown to get all accumulated data */
#define TSMUX_TRANSFORM_FLAG_FLUSH	1

typedef struct {
	/* Size of the structure in bytes */
	MME_UINT StructSize;

	MME_UINT TransformFlags;

} TSMUX_TransformParam_t;

/* --- TSMUX_IndexRecord_t --- */

typedef struct {
	MME_UINT PacketOffset;
	MME_UINT Program;
	MME_UINT Stream;
	MME_UINT IndexIdentifierSize;
	unsigned char IndexIdentifier[TSMUX_MAXIMUM_INDEX_IDENTIFIER_SIZE];
	unsigned long long pts;
} TSMUX_IndexRecord_t;

/* --- TSMUX_CommandStatus_t --- */

/* Used to indicate that some data is unconsumed */
#define TSMUX_TRANSFORM_OUTPUT_FLAG_UNCONSUMED_DATA	1

typedef struct {
	/* Size of the structure in bytes */
	MME_UINT StructSize;
	/* TSMUX_OUTPUT_OVERFLOW - How large an output buffer is needed */
	MME_UINT OutputBufferSizeNeeded;
	/* TSMUX_INPUT_UNDERFLOW - Program and stream Id with shortage of data
	 * for output */
	MME_UINT DataUnderflowFirstProgram;
	MME_UINT DataUnderflowFirstStream;

	/* Data and flags coming back from the transform */
	/* Total used size */
	MME_UINT OutputSize;
	/* Offset in 90Khz ticks from the start of the multiplex */
	TSMUX_MME_TIMESTAMP OffsetFromStart;
	/* Length over which to transmit in 90Khz ticks */
	MME_UINT OutputDuration;
	MME_UINT Bitrate;
	/* The PCR value for this output */
	TSMUX_MME_TIMESTAMP PCR;

	MME_UINT TransformOutputFlags;

	/* The expected levels of the individual bit buffers at the end of this
	 * PCR period */
	MME_UINT
		DecoderBitBufferLevels[TSMUX_MAX_PROGRAMS][TSMUX_MAX_STREAMS_PER_PROGRAM];
	/* Index records of buffers which start being incorporated in this
	 * PCR period */
	MME_UINT NumberOfIndexRecords;
	TSMUX_IndexRecord_t IndexRecords[TSMUX_MAX_BUFFERS];
	/* Buffers completed during this transform */
	MME_UINT CompletedBufferCount;
	MME_UINT CompletedBuffers[TSMUX_MAX_BUFFERS];
	/* How long the transform took to execute */
	MME_UINT TransformDurationMs;
	/* Total TS packets have been out for this output */
	MME_UINT TSPacketsOut;
} TSMUX_CommandStatus_t;

/* --- TSMUX_Error_t --- */

typedef enum {
	/* The firmware was successful. */
	TSMUX_NO_ERROR = TSMUX_BASE,

	/* The firmware was not successful. */
	TSMUX_GENERIC_ERROR,
	TSMUX_INVALID_PARAMETER,
	/* Command specifies an invalid program or stream ID (out of range) */
	TSMUX_ID_OUT_OF_RANGE,

	/* Set global parameters error codes */
	/* Unable to generate PAT/PMT/SDT for stream type */
	TSMUX_UNSUPPORTED_TSMUX_STREAM_TYPE,
	/* Add program or stream, when it is already added */
	TSMUX_ID_IN_USE,
	/* on remove stream or program, the ID was not recognised */
	TSMUX_UNRECOGNISED_ID,

	/* Send buffers error codes */
	/* Unable to record buffer in list */
	TSMUX_INPUT_OVERFLOW,
	/* A section cannot repeat more frequently than its duration */
	TSMUX_REPEAT_INTERVAL_EXCEEDS_DURATION,
	/* Pes packet excedes the size of the bit buffer - impossible to
	 * multiplex */
	TSMUX_BIT_BUFFER_VIOLATION,
	/* DTS is widely at varience with last DTS on this stream, or DTS
	 * values of other streams */
	TSMUX_DTS_INTEGRITY_FAILURE,

	/* Transform error codes */
	/* Transform output buffer too small */
	TSMUX_OUTPUT_OVERFLOW,
	/* Insufficient input for transform */
	TSMUX_INPUT_UNDERFLOW,
	/* Failure to generate a multiplex that meets the DTS delivery
	 * schedule */
	/* NOTE transform still completes. */
	TSMUX_MULTIPLEX_DTS_VIOLATION,

	/* No PCR pid for a program - either no pid at all, or only stream
	 * based pids for other programs */
	TSMUX_NO_PCR_PID_SPECIFIED,

	/* Transform or send buffers errors */
	/* All buffers in use on send buffers or transform command */
	TSMUX_ALL_BUFFERS_IN_USE,
	/* All scatter pages in use on send buffers or transform command */
	TSMUX_ALL_SCATTER_PAGES_IN_USE,
	/* Out of internal memory */
	TSMUX_NO_MEMORY,

} TSMUX_Error_t;

#endif /* #ifndef __TSMUX_TRANSFORMER_TYPES_H */

/* //////////////////////////////////////////////////////////////////////// */
/* Nothing but Doxygen */
/* */

/* ------------------------------------------------------------------------ */
/* Capabilities structure */


/*! \struct TSMUX_TransformerCapability_t
	Capability structure.
*/
/*! \var TSMUX_TransformerCapability_t::StructSize
	Size of the structure in bytes
*/
/*! \var TSMUX_TransformerCapability_t::ApiVersion
		Api version, coded as 1.0 => 10
*/
/*! \var TSMUX_TransformerCapability_t::MaximumNumberOfPrograms
		The maximum number of programs that can be supported in
		this implementation.
*/
/*! \var TSMUX_TransformerCapability_t::MaximumNumberOfStreams
		The maximum number of streams that any program can have in
		this implementation
*/
/*! \var TSMUX_TransformerCapability_t::MaximumNumberOfBuffersPerStream
		The maximum number of buffers that can be queued to a stream
		in this implementation
*/

/* ------------------------------------------------------------------------- */
/*    Index record */

/*! \struct TSMUX_IndexRecord_t
	Structure defining an index record. Index records record packet offset
	from the start of a stream, that a particular buffer begins to appear
	in the stream.
	The record identifies the buffer by the program, stream, and identifier
	supplied when the record was submitted to the multiplexor.
*/

/*! \var TSMUX_CommandStatus_t::PacketOffset
		The packet offset from the start of the stream to which this
		index record applies.
*/
/*! \var TSMUX_SendBuffersParam_t::Program
	Identify the program that the index record applies to
*/
/*! \var TSMUX_SendBuffersParam_t::Stream
	Identify the stream that the index record applies to
*/
/*! \var TSMUX_CommandStatus_t::IndexIdentifierSize
		Size of the identifier for the buffer that this record applies
		to.
*/
/*! \var TSMUX_CommandStatus_t::IndexIdentifier
		Buffer identifier that this record applies to
*/

/* ------------------------------------------------------------------------- */
/*    Status structure */

/*! \def TSMUX_TRANSFORM_OUTPUT_FLAG_UNCONSUMED_DATA
		Returned flag from transform indicating there is data as
		yet unconsumed.
*/
/*! \struct TSMUX_CommandStatus_t
	The command status structure is used to return helpful information
	in an error condition.
*/

/*! \var TSMUX_CommandStatus_t::StructSize
		Size of the structure in bytes
*/
/*! \var TSMUX_CommandStatus_t::OutputBufferSizeNeeded
		For too small a buffer supplied to the transform command,
		this value indiocates what what is needed.
*/
/*! \var TSMUX_CommandStatus_t::DataUnderflowFirstProgram
		For insufficient data to output a PCR period, this indicates
		the first program that was known to be short of data.
*/
/*! \var TSMUX_CommandStatus_t::DataUnderflowFirstStream
		For insufficient data to output a PCR period, this indicates
		the first stream that was known to be short of data.
*/
/*! \var TSMUX_CommandStatus_t::OffsetFromStart
		Offset in 90Khz ticks from the start of the stream
		(IE PCR - First PCR).
*/
/*! \var TSMUX_CommandStatus_t::OutputDuration
		Time over which to transmit in 90Khz ticks.
*/
/*! \var TSMUX_CommandStatus_t::Bitrate
		Bitrate for this output, for variable bitrate multiplexes
*/
/*! \var TSMUX_CommandStatus_t::PCR
		PCR value for this output. Expressed in 90Khz ticks
*/
/*! \var TSMUX_CommandStatus_t::TransformOutputFlags
		Flags relating to current state, currently we only have
		TRANSFORM_OUTPUT_FLAG_UNCONSUMED_DATA. It indicates there
		is still data queued but not yet output. Very useful during
		a flush operation prior to closedown.
*/
/*! \var TSMUX_CommandStatus_t::DecoderBitBufferLevels
		A 2 D array indicating the expected levels of the individual
		bit buffers for each program.stream at the end of this PCR
		period.
*/
/*! \var TSMUX_CommandStatus_t::NumberOfIndexRecords
		Number of index records generated during this transform.
*/
/*! \var TSMUX_CommandStatus_t::IndexRecords
		Index records generated during this transform.
*/
/*! \var TSMUX_CommandStatus_t::TransformDurationMs
		A measure of the elapsed time taken to perform the transform.
*/

/* ------------------------------------------------------------------------- */
/* Init params structure */

/*! \def TSMUX_TABLE_GENERATION_NONE
		No table generation
*/
/*! \def TSMUX_TABLE_GENERATION_PAT_PMT
		Generate Pat/Pmt tables
*/
/*! \def TSMUX_TABLE_GENERATION_SDT
		Generate Sdt tables
*/
/*! \struct TSMUX_InitTransformerParam_t
	The transformer takes initialization parameters via the structure.
*/
/*! \var TSMUX_InitTransformerParam_t::StructSize
		Size of the structure in bytes
*/
/*! \var TSMUX_InitTransformerParam_t::PcrPeriod90KhzTicks
		Specify the PCR period in 90 Khz ticks, typically between
		1500 and 9000. Default is 9000. NOTE greater than 9000 is
		non-compliant in DVB
*/
/*! \var TSMUX_InitTransformerParam_t::GeneratePcrStream
		Flag that causes PCRs to be emitted on a specific
		pid with no other data on it
*/
/*! \var TSMUX_InitTransformerParam_t::PcrPid
		Specific pid on which to emit PCRs.
		Default is 0x1ffe.
*/
/*! \var TSMUX_InitTransformerParam_t::TableGenerationFlags
		Specify what tables to generate and incorporate in the mux.
		Options are NONE, PAT/PMT, and SDTs. NOTE these will be
		output once per PCR period.
*/
/*! \var TSMUX_InitTransformerParam_t::TablePeriod90KhzTicks
		Specify the table inclusion period in 90 Khz ticks, typically
	between
		2250 and 9000. Default is PCr period value. NOTE greater than
		9000 is non-compliant in DVB, as a value less than 2250.
*/
/*! \var TSMUX_InitTransformerParam_t::TransportStreamId
		Specify the transport stream ID to be included in PAT table.
		Must be in range 0..0xffff. Default is 0.
*/
/*! \var TSMUX_InitTransformerParam_t::FixedBitrate
		Flag indicating whether or not a fixed bitrate stream is
		being generated. 0=>No, non-0=>Yes.
*/
/*! \var TSMUX_InitTransformerParam_t::Bitrate
		If fixed bitrate stream, this variable specifies the
		bitrate.
		For non-fixed boitrate streams this is the constraint on
		the peak bitrate.
*/
/*! \var TSMUX_InitTransformerParam_t::RecordIndexIdentifiers
		Flag indicating whether or not index records are to be
		generated during transforms.
*/

/* ------------------------------------------------------------------------- */
/*    Set global parameters structure */

/*! \def TSMUX_COMMAND_ADD_PROGRAM
		The global command adds a program to the multiplex
*/
/*! \def TSMUX_COMMAND_REMOVE_PROGRAM
		The global command removes a program from the multiplex
*/
/*! \def TSMUX_COMMAND_ADD_STREAM
		The global command adds a stream to a program in the multiplex
*/
/*! \def TSMUX_COMMAND_REMOVE_STREAM
		The global command removes a stream from a program in the
		multiplex
*/
/*! \struct TSMUX_SetGlobalParams_t
	The transformer has global multiplex parameters, these take two forms,
	those transmitted during the open/initialization, and those affected
	by the set global parameters command. This function has 4 sub-functions
	that manipulate the components within the multiplex.
*/
/*! \var TSMUX_SetGlobalParams_t::StructSize
		Size of the structure in bytes
*/
/*! \var TSMUX_SetGlobalParams_t::CommandCode
		Code indicating the sub command, as add/remove program,
		and add/remove stream from program.
*/
/*! \var TSMUX_SetGlobalParams_t::AddProgram
		Sub-command structure
*/
/*! \var TSMUX_SetGlobalParams_t::RemoveProgram
		Sub-command structure
*/
/*! \var TSMUX_SetGlobalParams_t::AddStream
		Sub-command structure
*/
/*! \var TSMUX_SetGlobalParams_t::RemoveStream
		Sub-command structure
*/

/* ------------------------------------------------------------------------- */
/*    Add program structure */

/*! \struct TSMUX_AddProgram_t
		Sub-command structure for add program.
*/
/*! \var TSMUX_AddProgram_t::ProgramId
		A number in the range 0..(MaximumNumberOfPrograms-1)
*/
/*! \var TSMUX_AddProgram_t::TablesProgram
		Flag used to indicate this is a tables program, that normally
		contains only sections. Will not appear in any generated
		PAT/PMT table.
		Can be used for other content, but not really expected to be.
*/
/*! \var TSMUX_AddProgram_t::ProgramNumber
		A number in the range 0..ffff
		When PAT generation is desired, this value will be used in
		the Program Number field. In general 0 should only be used
		for a section stream incorporating an NIT table (dvb standard).
		It is expected that 0 will not be used. Default value is
		ProgramId.
*/
/*! \var TSMUX_AddProgram_t::PMTPid
		A number in the range 1..1ffd
		When PMT generation is desired, this value will control the pid
		on which the PMT can be found. Default is 0x30+ProgramId
*/
/*! \var TSMUX_AddProgram_t::StreamsMayBeScrambled
		Flag used in simple SDT generation.
*/
/*! \var TSMUX_AddProgram_t::ProviderName
		A name for the provider, used only if the multplexor is
		generating SDT tables.
*/
/*! \var TSMUX_AddProgram_t::ServiceName
		A name for the service, used only if the multplexor is
		generating SDT tables.
*/
/*! \var TSMUX_AddProgram_t::OptionalDescriptorSize
		Size of the optional descriptor, only relevent if the
		multiplexor is generating PAT/PMT tables.
*/
/*! \var TSMUX_AddProgram_t::Descriptor
		String of bytes containing the descriptor if length not zero.
*/

/* ------------------------------------------------------------------------- */
/*    Remove program structure */

/*! \struct TSMUX_RemoveProgram_t
		Sub-command structure for remove program.
*/
/*! \var TSMUX_RemoveProgram_t::ProgramId
		Identifier of the program to remove from the multiplex.
*/

/* ------------------------------------------------------------------------- */
/*    Add stream structure */

/*! \def TSMUX_STREAM_CONTENT_PES
		Specify stream content as pes data.
*/
/*! \def TSMUX_STREAM_CONTENT_SECTION
		Specify stream content as section data.
*/
/*! \def TSMUX_STREAM_TYPE_RESERVED
		specify stream type as RESERVED
*/
/*! \def TSMUX_STREAM_TYPE_VIDEO_MPEG1
		specify stream type as video MPEG1
*/
/*! \def TSMUX_STREAM_TYPE_VIDEO_MPEG2
		specify stream type as video MPEG2
*/
/*! \def TSMUX_STREAM_TYPE_AUDIO_MPEG1
		specify stream type as audio MPEG1
*/
/*! \def TSMUX_STREAM_TYPE_AUDIO_MPEG2
		specify stream type as audio MPEG2
*/
/*! \def TSMUX_STREAM_TYPE_PRIVATE_SECTIONS
		specify stream type as private sections
*/
/*! \def TSMUX_STREAM_TYPE_PRIVATE_DATA
		specify stream type as private data
*/
/*! \def TSMUX_STREAM_TYPE_MHEG
		specify stream type as MHEG
*/
/*! \def TSMUX_STREAM_TYPE_DSMCC
		specify stream type as DSMCC
*/
/*! \def TSMUX_STREAM_TYPE_H222_1
		specify stream type as H222 1
*/
/*! \def TSMUX_STREAM_TYPE_AUDIO_AAC
		specify stream type as audio AAC
*/
/*! \def TSMUX_STREAM_TYPE_VIDEO_MPEG4
		specify stream type as video MPEG4
*/
/*! \def TSMUX_STREAM_TYPE_VIDEO_H264
		specify stream type as video H264
*/
/*! \def TSMUX_STREAM_TYPE_PS_AUDIO_AC3
		specify stream type as audio AC3
*/
/*! \def TSMUX_STREAM_TYPE_PS_AUDIO_DTS
		specify stream type as audio DTS
*/
/*! \def TSMUX_STREAM_TYPE_PS_AUDIO_LPCM
		specify stream type as audio LPCM
*/
/*! \def TSMUX_STREAM_TYPE_PS_DVD_SUBPICTURE
		specify stream type as DVD sub-picture
*/
/*! \def TSMUX_STREAM_TYPE_VIDEO_DIRAC
		specify stream type as video DIRAC
*/
/*! \struct TSMUX_AddStream_t
		Sub-command structure for add stream.
*/
/*! \var TSMUX_AddStream_t::StreamId
		A number in the range 0..(MaximumNumberOfStreams-1)
*/
/*! \var TSMUX_AddStream_t::IncorporatePcr
		Flag indicating that this stream should also contain PCR
		packets.
*/
/*! \var TSMUX_AddStream_t::StreamContent
		Variable indicating the gross content type, pes or section
		data. One of the defined values TSMUX_STREAM_CONTENT_...
*/
/*! \var TSMUX_AddStream_t::StreamPid
		Pid onto which this stream is to be muxed. Default is
		0x100 * (ProgramId+1) + StreamId
*/
/*! \var TSMUX_AddStream_t::StreamType
		Deeper stream content type used in PAT/PMT generation,
		indicates the actual codec to be used to decode the stream.
		One of TSMUX_STREAM_TYPE_... these are DVB standard values.
*/
/*! \var TSMUX_AddStream_t::OptionalDescriptorSize
		Size of the optional descriptor, only relevent if the
		multiplexor is generating PAT/PMT tables with descriptors.
		NOTE this can be zero even if descriptors are being generated.
*/
/*! \var TSMUX_AddStream_t::Descriptor
		String of bytes containing the descriptor if length not zero.
*/
/*! \var TSMUX_AddStream_t::DTSIntegrityThreshold90KhzTicks
		Every buffer queued has its DTS value checked to ensure that
		it is sensible with respect to the other DTS values in the
		system. This control specifies how much a DTS may change
		between packets. Default is 1 second, a value of zero
		implies infinity (IE no checking).
*/
/*! \var TSMUX_AddStream_t::DecoderBitBufferSize
		Size of T-STD buffer in bits. Could have been bytes, but
		everyone calls it the bit buffer, so I left it as bits.
*/
/*! \var TSMUX_AddStream_t::MultiplexAheadLimit90KhzTicks
		For highly variable rate streams, this limits how early a
		packet is needed for inclusion in the mux. This allows the bit
		buffer to run low during low bitrate portions of a stream.
		When checking that sufficient data is queued to the
		multiplexor, checking stops after finding the first packet with
		a DTS greater than this time after the end of the PCR period.
		If available such packets may still be included in the mux for
		that period, but they are not required.
*/

/* ------------------------------------------------------------------------- */
/*    Remove program structure */

/*! \struct TSMUX_RemoveStream_t
		Sub-command structure for remove stream.
*/
/*! \var TSMUX_RemoveStream_t::ProgramId
		Identifier of the program from which to remove the specified
		stream.
*/
/*! \var TSMUX_RemoveStream_t::StreamId
		Identifier of the stream to remove from the program.
*/

/* ------------------------------------------------------------------------- */
/*    Dan's send buffers one */

/* --- Doxycomment for TSMUX_SendBuffersParam_t --- */

/*! \def TSMUX_BUFFER_FLAG_NO_DATA
    Used to fill time holes. Buffers with this flag set, should contain no
    other information, they are deemed to be in effect from after the last
    existing buffer, until the next buffer is transmitted.
*/
/*! \def TSMUX_BUFFER_FLAG_DISCONTINUITY
    Mark a discontinuity at the beginning of the provided buffer.
*/
/*! \def TSMUX_BUFFER_FLAG_TRANSPORT_SCRAMBLED
    Indicate data is to be marked as scrambled in transport packet headers.
*/
/*! \def TSMUX_BUFFER_FLAG_KEY_PARITY
    Only relevent if scrambled.
*/
/*! \def TSMUX_BUFFER_FLAG_REPEATING
    Indicate section buffer is to inserted into the stream at regular
    intervals.
*/
/*! \def TSMUX_BUFFER_FLAG_REQUEST_DIT_INSERTION
    Indicate that a DIT section should appear in the stream before this buffer
    is output.
*/
/*! \struct TSMUX_SendBuffersParam_t
    The transformer takes one pes/section buffer at time, which may
    consist of one or more scatter pages.
*/
/*! \var TSMUX_SendBuffersParam_t::StructSize
    Size of the structure in bytes.
*/
/*! \var TSMUX_SendBuffersParam_t::ProgramId
    Identify the program that the buffer applies to
*/
/*! \var TSMUX_SendBuffersParam_t::StreamId
    Identify the stream that the buffer applies to
*/
/*! \var TSMUX_SendBuffersParam_t::DTS
    Decode timestamp, time (expressed in 90Khz ticks) at which the
    buffer has to be completely available in the bit buffer.
*/
/*! \var TSMUX_SendBuffersParam_t::DTSDuration
    Time period represented by this buffer. This is used to
    deduce the ideal position in the stream for each transport
    packet that makes up the buffer. Specifying this as zero, will
    force the multiplex algorithm to attempt to pack the data
    in consecutive transport packets.
*/
/*! \var TSMUX_SendBuffersParam_t::BufferFlags;
    Flags that impact the multiplexing of this buffer such as
    indicating the presence of data, signalling a discontinuity,
    informing the multiplexor of the scrambling state of the
    buffer, and whether or not the buffer is to be repeated
    in the case of a section stream.
*/
/*! \var TSMUX_SendBuffersParam_t::RepeatInterval90KhzTicks
    For section streams that are to be repeated, this variable
    indicates the period between repetitions. NOTE this must be
    longer than the duration, as we cannot restart a section
    transmission before finishing it.
*/
/*! \var TSMUX_SendBuffersParam_t::DITTransitionFlagValue
    For insertion of a DIT table, this holds the transition flag
    value (0/1). There is no default,
*/
/*! \var TSMUX_SendBuffersParam_t::IndexIdentifierSize
    If Index records are being generated, this field specifies the size of any
    index identifier associated with this buffer. NOTE if this is zero
    no index record will be generated when this buffer is incorporated
    into the multiplex.
*/
/*! \var TSMUX_SendBuffersParam_t::IndexIdentifier
    Index identifier associated with this buffer (see IndexIdentifierSize).
*/

/* ------------------------------------------------------------------------- */
/*    Transform structure */

/*! \def TSMUX_TRANSFORM_FLAG_FLUSH
	Inform transform to output what it has without checking that the data
	fills a full PCR period.
*/
/*! \struct TSMUX_TransformParam_t
	The transformer will attempt to output one PCR periods worth of data on
	each transform command. An error will occur if there is insufficient
	buffered data.
*/
/*! \var TSMUX_TransformParam_t::StructSize
		Size of the structure in bytes
*/
/*! \var TSMUX_TransformParam_t::TransformFlags
		Specify any flags to the transform, currently we only have
		TRANSFORM_FLAG_FLUSH, which informs the transform not to
		complain about any shortage of data, and to output what we
		have, though still only upto 1 PCR period.
*/

/* ------------------------------------------------------------------------- */

/*! \mainpage

\section intro Introduction

The complete inferface to the transport stream multiplexor is described in the
following header file:

- TsmuxTransformerTypes.h

\section usage Typical usage

Typical usage of this transformer by the driver is described by the following
pseudocode:
<pre>
Mark current Bit buffer levels as 0

Initialize - Default to generated simple PAT/PMT/SDT

Add Program 0
	Add stream  0 - Audio
	Add stream  1 - Video

Loop

    PAR
	0 - Send Buffers - enough audio for one PCR period +
			Bit buffer size - Bit buffer level
	1 - Send Buffers - enough video for one PCR period
			Bit buffer size - Bit buffer level

	Issue Transform
	Retrieve current bit buffer levels

Do
	Issue transform with flush set
While (Transform flags TSMUX_TRANSFORM_OUTPUT_FLAG_UNCONSUMED_DATA set)

Terminate
<\pre>
*/

