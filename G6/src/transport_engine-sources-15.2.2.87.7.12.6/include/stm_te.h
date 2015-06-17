/******************************************************************************
Copyright (C) 2011, 2012 STMicroelectronics. All Rights Reserved.

This file is part of the Transport Engine Library.

Transport Engine is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published by
the Free Software Foundation.

Transport Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Transport Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Transport Engine Library may alternatively be licensed under a proprietary
license from ST.

Source file name : stm_te.h

Defines the stm_te API
******************************************************************************/
#ifndef _STM_TE_H
#define _STM_TE_H

#include "stm_common.h"
#include "stm_registry.h"

#define STM_TE_PID_ALL       0x2000
#define STM_TE_PID_NONE      0xE000
/* Maximum length of TSMUX/TSG_FILTER DESCRIPTORS and strings */
#define STM_TE_TSMUX_MAX_DESCRIPTOR 16


/* Opaque transport engine object */
typedef void *stm_te_object_h;

typedef uint16_t stm_te_pid_t;

typedef struct stm_te_caps_s {
	unsigned int max_demuxes;
	unsigned int max_pid_filters;
	unsigned int max_output_filters;
	unsigned int max_tsmuxes;
	unsigned int max_tsg_filters;
	unsigned int max_tsg_filters_per_mux;
} stm_te_caps_t;

typedef enum stm_te_input_type_e {
	STM_TE_INPUT_TYPE_UNKNOWN,
	STM_TE_INPUT_TYPE_DVB,
	STM_TE_INPUT_TYPE_TTS,
} stm_te_input_type_t;

typedef enum stm_te_output_type_e {
	STM_TE_OUTPUT_TYPE_DVB,
	STM_TE_OUTPUT_TYPE_TTS,
	STM_TE_OUTPUT_TYPE_SAME,
} stm_te_output_type_t;

typedef enum stm_te_demux_ctrl_e {
	STM_TE_DEMUX_CNTRL_PACING_OUTPUT,
	STM_TE_DEMUX_CNTRL_INPUT_TYPE,
	STM_TE_DEMUX_CNTRL_DISCARD_DUPLICATE_PKTS,
	STM_TE_DEMUX_CNTRL_LAST,
} stm_te_demux_ctrl_t;

typedef enum stm_te_demux_compound_control_e {
	STM_TE_DEMUX_CNTRL_STATUS = STM_TE_DEMUX_CNTRL_LAST,
	STM_TE_DEMUX_COMPOUND_CNTRL_LAST,
} stm_te_demux_compound_ctrl_t;

typedef enum stm_te_tsmux_table_gen_e {
	STM_TE_TSMUX_CNTRL_TABLE_GEN_NONE = 0x00,
	STM_TE_TSMUX_CNTRL_TABLE_GEN_PAT_PMT = 0x01,
	STM_TE_TSMUX_CNTRL_TABLE_GEN_SDT = 0x04
} stm_te_tsmux_table_gen_flags_t;

typedef enum stm_te_tsmux_stop_mode_e {
	STM_TE_TSMUX_CNTRL_STOP_MODE_COMPLETE,
	STM_TE_TSMUX_CNTRL_STOP_MODE_FORCED,
} stm_te_tsmux_stop_mode_t;

typedef enum stm_te_tsmux_ctrl_e {
	/* TSMUX global controls */
	STM_TE_TSMUX_CNTRL_OUTPUT_TYPE,
	STM_TE_TSMUX_CNTRL_PCR_PERIOD,
	STM_TE_TSMUX_CNTRL_GEN_PCR_STREAM,
	STM_TE_TSMUX_CNTRL_PCR_PID,
	STM_TE_TSMUX_CNTRL_TABLE_GEN,
	STM_TE_TSMUX_CNTRL_TABLE_PERIOD,
	STM_TE_TSMUX_CNTRL_TS_ID,
	STM_TE_TSMUX_CNTRL_BIT_RATE_IS_CONSTANT,
	STM_TE_TSMUX_CNTRL_BIT_RATE,
	STM_TE_TSMUX_CNTRL_NEXT_STREAM_ID,
	STM_TE_TSMUX_CNTRL_NEXT_SEC_STREAM_ID,
	/* Program controls */
	STM_TE_TSMUX_CNTRL_PROGRAM_NUMBER,
	STM_TE_TSMUX_CNTRL_PMT_PID,
	STM_TE_TSMUX_CNTRL_STOP_MODE,
	STM_TE_TSMUX_CNTRL_LAST,
} stm_te_tsmux_ctrl_t;

typedef enum stm_te_tsmux_compound_control_e {
	/* TSMux compound controls */
	STM_TE_TSMUX_CNTRL_SDT_PROV_NAME = STM_TE_TSMUX_CNTRL_LAST,
	STM_TE_TSMUX_CNTRL_SDT_SERV_NAME,
	STM_TE_TSMUX_CNTRL_PMT_DESCRIPTOR,
	STM_TE_TSMUX_COMPOUND_CNTRL_LAST,
} stm_te_tsmux_compound_ctrl_t;

enum stm_te_tsmux_events {
	STM_TE_TSMUX_TSG_EOS_EVENT = 1,
};

typedef enum stm_te_filter_e {
	STM_TE_PID_FILTER,
	STM_TE_TS_FILTER,
	STM_TE_PES_FILTER,
	STM_TE_SECTION_FILTER,
	STM_TE_PCR_FILTER,
	STM_TE_TS_INDEX_FILTER,
	STM_TE_ECM_FILTER,
	STM_TE_TSG_FILTER,
	STM_TE_TSG_SEC_FILTER,
	STM_TE_TSG_INDEX_FILTER,
	STM_TE_LAST_FILTER,
} stm_te_filter_t;

/* control values for STM_TE_OUTPUT_FILTER_CONTROL_OVERFLOW_BEHAVIOUR */
enum {
	STM_TE_FILTER_CONTROL_OVERFLOW_DISCARDS_OLD_DATA,
	STM_TE_FILTER_CONTROL_OVERFLOW_DISCARDS_NEW_DATA,
};

/* control values for STM_TE_OUTPUT_FILTER_CONTROL_FLUSHING_BEHAVIOUR */
enum {
	STM_TE_FILTER_CONTROL_FLUSH_NONE           = 0x00,
	STM_TE_FILTER_CONTROL_FLUSH_ON_DETACH      = 0x01,
	STM_TE_FILTER_CONTROL_FLUSH_ON_PID_CHANGE  = 0x02,
};

enum {
	STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE_RESERVED		= 0x00,
	STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE_VIDEO_MPEG1	= 0x01,
	STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE_VIDEO_MPEG2	= 0x02,
	STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE_AUDIO_MPEG1	= 0x03,
	STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE_AUDIO_MPEG2	= 0x04,
	STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE_PRIVATE_SECTIONS	= 0x05,
	STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE_PRIVATE_DATA	= 0x06,
	STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE_MHEG		= 0x07,
	STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE_DSMCC		= 0x08,
	STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE_H222_1		= 0x09,

	STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE_AUDIO_AAC		= 0x0f,
	STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE_VIDEO_MPEG4	= 0x10,
	STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE_VIDEO_H264	= 0x1b,

	STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE_PS_AUDIO_AC3	= 0x81,
	STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE_PS_AUDIO_DTS	= 0x8a,
	STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE_PS_AUDIO_LPCM	= 0x8b,
	STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE_PS_DVD_SUBPICTURE	= 0xff,

	STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE_VIDEO_DIRAC	= 0xD1,
};

typedef enum stm_te_filter_control_e {
	/* Generic input filter controls */
	STM_TE_INPUT_FILTER_CONTROL_PID,

	/* PID insertion/replacement controls */
	STM_TE_PID_INS_FILTER_CONTROL_DATA,
	STM_TE_PID_INS_FILTER_CONTROL_FREQ,
	STM_TE_PID_INS_FILTER_CONTROL_TRIG,

	/* Generic output filter controls */
	STM_TE_OUTPUT_FILTER_CONTROL_BUFFER_SIZE,
	STM_TE_OUTPUT_FILTER_CONTROL_OVERFLOW_BEHAVIOUR,
	STM_TE_OUTPUT_FILTER_CONTROL_FLUSHING_BEHAVIOUR,
	STM_TE_OUTPUT_FILTER_CONTROL_ERROR_RECOVERY,
	STM_TE_OUTPUT_FILTER_CONTROL_READ_IN_QUANTISATION_UNITS,
	STM_TE_OUTPUT_FILTER_CONTROL_FLUSH,
	STM_TE_OUTPUT_FILTER_CONTROL_PAUSE,

	/* TS filter specific controls */
	STM_TE_TS_FILTER_CONTROL_DLNA_OUTPUT,
	STM_TE_TS_FILTER_CONTROL_SECURE_OUTPUT,

	/* Section filter specific controls */
	STM_TE_SECTION_FILTER_CONTROL_REPEATED,
	STM_TE_SECTION_FILTER_CONTROL_FORCE_CRC_CHECK,
	STM_TE_SECTION_FILTER_CONTROL_DISCARD_ON_CRC_ERROR,
	STM_TE_SECTION_FILTER_CONTROL_VERSION_NOT_MATCH,
	STM_TE_SECTION_FILTER_CONTROL_PROPRIETARY_1,
	STM_TE_SECTION_FILTER_CONTROL_ECM,
	STM_TE_SECTION_FILTER_CONTROL_PATTERN,
	STM_TE_SECTION_FILTER_CONTROL_MASK,
	STM_TE_SECTION_FILTER_CONTROL_POS_NEG_PATTERN,
	STM_TE_SECTION_FILTER_CONTROL_POS_NEG_ENABLED,

	/* PES filter specific controls */
	STM_TE_PES_FILTER_CONTROL_STREAM_ID_FILTER,

	/* PCR filter specific controls */
	__dep_STM_TE_PCR_FILTER_CONTROL_CLK_RCV_LOOP,

	/* Secondary PID controls */
	STM_TE_PID_FILTER_CONTROL_SECONDARY_PID,

	/* TSG filter specific controls */
	STM_TE_TSG_FILTER_CONTROL_INCLUDE_PCR,
	STM_TE_TSG_FILTER_CONTROL_STREAM_PID,
	STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE,
	STM_TE_TSG_FILTER_CONTROL_STREAM_IS_PES,
	STM_TE_TSG_FILTER_CONTROL_PES_STREAM_ID,
	STM_TE_TSG_FILTER_CONTROL_BIT_BUFFER_SIZE,
	STM_TE_TSG_FILTER_CONTROL_INCLUDE_RAP,

	STM_TE_TSG_SEC_FILTER_CONTROL_DATA,
	STM_TE_TSG_SEC_FILTER_CONTROL_FREQ,

	STM_TE_TSG_INDEX_FILTER_CONTROL_INDEX,
	STM_TE_TSG_FILTER_CONTROL_IGNORE_FIRST_DTS_CHECK,
	STM_TE_TSG_FILTER_CONTROL_STREAM_MANUAL_PAUSED,
	STM_TE_TSG_FILTER_CONTROL_IGNORE_AUTO_PAUSE,
	STM_TE_TSG_FILTER_CONTROL_SCATTERED_STREAM,
	STM_TE_TSG_FILTER_CONTROL_DTS_INTEGRITY_THRESHOLD,
	STM_TE_TSG_FILTER_CONTROL_MULTIPLEX_AHEAD_LIMIT,
	STM_TE_TSG_FILTER_CONTROL_DONT_WAIT_LIMIT,
	STM_TE_TSG_FILTER_CONTROL_DTS_DURATION,
	STM_TE_TSG_FILTER_CONTROL_NUM_INPUT_BUFFERS,
	/* LAST filter control*/
	STM_TE_FILTER_CONTROL_LAST,
} stm_te_filter_ctrl_t;

typedef enum stm_te_filter_compound_control_e {
	/* Generic filter controls */
	__deprecated_STM_TE_FILTER_CONTROL_STATUS = STM_TE_FILTER_CONTROL_LAST,

	/* Input filter controls */
	STM_TE_INPUT_FILTER_CONTROL_STATUS,

	/* Output filter controls */
	STM_TE_OUTPUT_FILTER_CONTROL_STATUS,

	/* TS Index filter specific controls */
	STM_TE_TS_INDEX_FILTER_CONTROL_INDEX_SET,
	/* Input filter corruption control*/
	STM_TE_PID_FILTER_CONTROL_PKT_CORRUPTION,
	/* TSG filter specific controls */
	STM_TE_TSG_FILTER_CONTROL_STREAM_DESCRIPTOR,
	STM_TE_FILTER_COMPOUND_CONTROL_LAST,
} stm_te_filter_compound_ctrl_t;

typedef enum {
	STM_TE_INDEX_PUSI                = 0x00000008,
	STM_TE_INDEX_TO_EVEN_SCRAM       = 0x00000010,
	STM_TE_INDEX_TO_ODD_SCRAM        = 0x00000020,
	STM_TE_INDEX_SCRAM_TO_CLEAR      = 0x00000040,
	STM_TE_INDEX_PTS                 = 0x00000100,
	STM_TE_INDEX_FIRST_REC_PACKET    = 0x00200000,
	STM_TE_INDEX_START_CODE          = 0x00400000,
	STM_TE_INDEX_ADAPTATION_EXT      = 0x01000000,
	STM_TE_INDEX_TS_PRIVATE_DATA     = 0x02000000,
	STM_TE_INDEX_SPLICING_POINT      = 0x04000000,
	STM_TE_INDEX_OPCR                = 0x08000000,
	STM_TE_INDEX_PCR                 = 0x10000000,
	STM_TE_INDEX_ES_PRIORITY         = 0x20000000,
	STM_TE_INDEX_RANDOM_ACCESS       = 0x40000000,
	STM_TE_INDEX_DISCONTINUITY       = 0x80000000
} stm_te_ts_index_definition_t;

typedef enum {
	STM_TE_SECTION_FILTER_EVENT_CRC_ERROR = 0x00000001
} stm_te_filter_event_t;


/* Compound control structures */
typedef struct {
	stm_te_ts_index_definition_t    index_definition;
	uint8_t                         number_of_start_codes;
	uint8_t                        *start_codes;
} stm_te_ts_index_set_params_t;

typedef struct {
	uint64_t	pcr;
	union {
		uint64_t system_time;
		__attribute__ ((deprecated)) uint64_t clk;
	};
	uint32_t	flags;
	uint32_t	packet_count;
	uint8_t		mpeg_start_code;
	uint8_t		mpeg_start_code_offset;
	uint16_t	number_of_extra_bytes;
	uint16_t	reserved;
	uint16_t	pid;
} stm_te_ts_index_data_t;

typedef struct {
	bool enable;
	/*offset, from where the TS packet needs to be overwritten*/
	uint8_t offset;
	/* pattern to be overwritten*/
	uint8_t value;
} stm_te_pid_filter_pkt_corruption_t;

/* The following struct should be used as the value when setting the tsmux
 * compound control STM_TE_TSMUX_CNTRL_PMT_DESCRIPTOR and the tsg_filter
 * control STM_TE_TSG_FILTER_CONTROL_STREAM_DESCRIPTOR */
typedef struct {
	uint8_t size;
	uint8_t descriptor[STM_TE_TSMUX_MAX_DESCRIPTOR];
} stm_te_tsmux_descriptor_t;

typedef enum {
	STM_TE_TSG_INDEX_PUSI                = 0x00000001,
	STM_TE_TSG_INDEX_PTS                 = 0x00000002,
	STM_TE_TSG_INDEX_I_FRAME             = 0x00000004,
	STM_TE_TSG_INDEX_B_FRAME             = 0x00000008,
	STM_TE_TSG_INDEX_P_FRAME             = 0x00000010,
	STM_TE_TSG_INDEX_DIT                 = 0x00000020,
	STM_TE_TSG_INDEX_RAP                 = 0x00000040,
	STM_TE_TSG_INDEX_STREAM_ID	     = 0x00000100,
	STM_TE_TSG_INDEX_STREAM_TYPE	     = 0x00000200,
	STM_TE_TSG_INDEX_PAT		     = 0x00000400,
	STM_TE_TSG_INDEX_PMT		     = 0x00000800,
	STM_TE_TSG_INDEX_SDT		     = 0x00001000,
} stm_te_tsg_index_definition_t;

typedef struct {
	uint64_t	pts;
	uint32_t	index;
	uint32_t	packet_count;
	uint16_t	pid;
	uint8_t		stream_id;
	uint8_t		stream_type;
	uint64_t	native_pts;
	uint8_t		reserved[4];
} stm_te_tsg_index_data_t;

typedef enum stm_te_secondary_pid_mode_e {
	STM_TE_SECONDARY_PID_SUBSTITUTION,
	STM_TE_SECONDARY_PID_INSERTION,
	STM_TE_SECONDARY_PID_INSERTDELETE,
} stm_te_secondary_pid_mode_t;

typedef struct {
	unsigned long long pcr;
	unsigned long long system_time;
} stm_te_pcr_t;

typedef struct {
	uint64_t system_time;
	uint32_t packet_count;
	uint32_t cc_errors;
	uint32_t tei_count;
	uint32_t input_packets;
	uint32_t output_packets;
	uint32_t crc_errors;
	uint32_t buffer_overflows;
	uint32_t utilization;
} stm_te_demux_stats_t;

typedef struct {
	uint64_t system_time;
	uint32_t packet_count;
} stm_te_input_filter_stats_t;

typedef struct {
	uint64_t system_time;
	uint32_t packet_count;
	uint32_t crc_errors;
	uint32_t buffer_overflows;
	uint32_t bytes_in_buffer;
} stm_te_output_filter_stats_t;

/* \deprecated This type is deprecated. Please use
 * stm_te_output_filter_stats_t.
 */
typedef stm_te_output_filter_stats_t stm_te_filter_status_t;

int __must_check stm_te_get_capabilities(stm_te_caps_t *capabilities);

int __must_check stm_te_demux_new(const char *name, stm_te_object_h *demux);

int __must_check stm_te_demux_delete(stm_te_object_h demux);

int __must_check stm_te_demux_start(stm_te_object_h demux);

int __must_check stm_te_demux_stop(stm_te_object_h demux);

int __must_check stm_te_demux_set_control(stm_te_object_h demux,
		stm_te_demux_ctrl_t selector, unsigned int value);

int __must_check stm_te_demux_get_control(stm_te_object_h demux,
		stm_te_demux_ctrl_t selector, unsigned int *value);

int __must_check stm_te_demux_get_compound_control(stm_te_object_h demux,
		stm_te_demux_compound_ctrl_t selector, void *value);

int __must_check stm_te_tsmux_new(const char *name, stm_te_object_h *tsmux);

int __must_check stm_te_tsmux_delete(stm_te_object_h tsmux);

int __must_check stm_te_tsmux_start(stm_te_object_h tsmux);

int __must_check stm_te_tsmux_stop(stm_te_object_h tsmux);

int __must_check stm_te_tsmux_set_control(stm_te_object_h tsmux,
		stm_te_tsmux_ctrl_t selector, unsigned int value);

int __must_check stm_te_tsmux_get_control(stm_te_object_h tsmux,
		stm_te_tsmux_ctrl_t selector, unsigned int *value);

int __must_check stm_te_tsmux_set_compound_control(stm_te_object_h tsmux,
		stm_te_tsmux_compound_ctrl_t selector, const void *value);

int __must_check stm_te_tsmux_get_compound_control(stm_te_object_h tsmux_h,
		stm_te_tsmux_compound_ctrl_t selector, void *value);

int __must_check stm_te_tsmux_attach(stm_te_object_h tsmux_h,
		stm_object_h target);

int __must_check stm_te_tsmux_detach(stm_te_object_h tsmux_h,
		stm_object_h target);

int __must_check stm_te_filter_delete(stm_te_object_h filter);

int __must_check stm_te_filter_set_control(stm_te_object_h filter,
		stm_te_filter_ctrl_t selector, unsigned int value);

int __must_check stm_te_filter_get_control(stm_te_object_h filter,
		stm_te_filter_ctrl_t selector, unsigned int *value);

int __must_check stm_te_filter_set_compound_control(stm_te_object_h filter,
		stm_te_filter_compound_ctrl_t selector, const void *value);

int __must_check stm_te_filter_get_compound_control(stm_te_object_h filter,
		stm_te_filter_compound_ctrl_t selector, void *value);

int __must_check stm_te_filter_attach(stm_te_object_h filter,
		stm_object_h target);

int __must_check stm_te_filter_detach(stm_te_object_h filter,
		stm_object_h target);

/* \deprecated	This function is deprecated. Please use
 * stm_te_filter_get_compound_control with STM_TE_OUTPUT_FILTER_CONTROL_STATUS
 * control
 */
int __must_check stm_te_filter_get_status(stm_te_object_h filter,
		stm_te_filter_status_t *status);

int __must_check stm_te_pid_filter_new(stm_te_object_h demux, stm_te_pid_t pid,
		stm_te_object_h *filter);

int __must_check stm_te_pid_filter_set_pid(stm_te_object_h pid_filter,
		stm_te_pid_t pid);

int __must_check stm_te_pid_filter_get_pid(stm_te_object_h pid_filter,
		stm_te_pid_t *pid);

int __must_check stm_te_pid_filter_set_pid_remap(stm_te_object_h pid_filter,
		stm_te_pid_t input_pid, stm_te_pid_t output_pid);

int __must_check stm_te_link_secondary_pid(stm_te_object_h demux_h,
		stm_te_pid_t primary_pid, stm_te_pid_t secondary_pid,
		stm_te_secondary_pid_mode_t mode);

int __must_check stm_te_unlink_secondary_pid(stm_te_object_h demux_h,
		stm_te_pid_t primary_pid);

int __must_check stm_te_output_filter_new(stm_te_object_h demux,
		stm_te_filter_t filter_type, stm_te_object_h *filter);

int __must_check stm_te_section_filter_set(stm_te_object_h filter,
		unsigned int length, unsigned char *filter_bytes,
		unsigned char *filter_masks);

int __must_check stm_te_section_filter_positive_negative_set(
		stm_te_object_h filter,
		unsigned int length, unsigned char *filter_bytes,
		unsigned char *filter_masks, unsigned char *pos_neg_pattern);

int __must_check stm_te_pid_ins_filter_new(stm_te_object_h demux,
		stm_te_pid_t pid, stm_te_object_h *filter);

int __must_check stm_te_pid_ins_filter_set(stm_te_object_h filter,
		unsigned char *data, unsigned int data_size,
		unsigned int freq_ms);

int __must_check stm_te_pid_ins_filter_trigger(stm_te_object_h filter_h);

int __must_check stm_te_pid_rep_filter_new(stm_te_object_h demux,
		stm_te_pid_t pid, stm_te_object_h *filter);

int __must_check stm_te_pid_rep_filter_set(stm_te_object_h filter,
		unsigned char *data, unsigned int data_size);

int __must_check stm_te_pid_attach(stm_te_object_h demux, stm_te_pid_t pid,
		void *target);

int __must_check stm_te_pid_detach(stm_te_object_h demux, stm_te_pid_t pid);

int __must_check stm_te_tsg_filter_new(stm_te_object_h tsmux,
		stm_te_object_h *filter);
int __must_check stm_te_tsg_sec_filter_new(stm_te_object_h tsmux,
		stm_te_object_h *filter);
int __must_check stm_te_tsg_sec_filter_set(stm_te_object_h filter,
		unsigned char *data, unsigned int data_size,
		unsigned int freq_ms);

int stm_te_tsg_index_filter_new(stm_te_object_h tsmux,
		stm_te_object_h *filter);

/* Deprecated types */
typedef stm_te_input_type_t te_input_type_t
		__attribute__((deprecated("Use stm_te_input_type_t")));
typedef stm_te_output_type_t te_output_type_t
		__attribute__((deprecated("Use stm_te_output_type_t")));

static const uint32_t STM_TE_DEMUX_TS_UNKNOWN
		__attribute__((deprecated("Use STM_TE_INPUT_TYPE_UNKNOWN")))
		= STM_TE_INPUT_TYPE_UNKNOWN;
static const uint32_t STM_TE_DEMUX_TS_INPUT
		__attribute__((deprecated("Use STM_TE_INPUT_TYPE_DVB")))
		= STM_TE_INPUT_TYPE_DVB;
static const uint32_t STM_TE_DEMUX_TS_DLNA_INPUT
		__attribute__((deprecated("Use STM_TE_INPUT_TYPE_TTS")))
		= STM_TE_INPUT_TYPE_TTS;

/* Deprecated constants */
static const uint32_t STM_TE_PCR_FILTER_CONTROL_CLK_RCV_LOOP
		__attribute__((deprecated)) =
		__dep_STM_TE_PCR_FILTER_CONTROL_CLK_RCV_LOOP;
static const uint32_t STM_TE_FILTER_CONTROL_CLK_RCV0
		__attribute__((deprecated)) = 0;
static const uint32_t STM_STM_TE_FILTER_CONTROL_CLK_RCV1
		__attribute__((deprecated)) = 1;
static const uint32_t STM_STM_TE_FILTER_CONTROL_CLK_RCV2
		__attribute__((deprecated)) = 2;
static const uint32_t STM_TE_FILTER_CONTROL_STATUS
		__attribute__((deprecated)) =
		__deprecated_STM_TE_FILTER_CONTROL_STATUS;



#endif /*_STM_TE_H*/
