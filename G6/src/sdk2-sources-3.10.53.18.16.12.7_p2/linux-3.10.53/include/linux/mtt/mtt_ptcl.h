/* \file mtt_datapotocol.h
   \brief The Multi-Target Trace (MTT) API data protocol

   This is the definition of the Multi-Target Trace API.
*/

#ifndef __mtt_PTCL_h
#define __mtt_PTCL_h

/* Brief The basic data types are defined in mtt_types.h */
#ifndef __KERNEL__
#include "../mtt_types.h"
#endif

/* This is a C interface, not a C++ interface. */
#ifdef __cplusplus
extern "C" {
#endif

/* Build revisions according SVN revision. */
#define MTT_PTCL_REV_STRING "$Rev: 595 $"
#if defined(__KERNEL__)
#include <linux/kernel.h>
#else
#include <stdlib.h> /* strtoul() */
#endif

#define MTT_PTCL_VER_MAJOR 1 /* Major rev. of binary protocol.*/
#define MTT_PTCL_VER_MINOR 0 /* Minor rev. of binary protocol.*/

/* Maximum name length of the API implementation comment
 (incl. terminating zero).*/
#define MTT_PTCL_IMP_COMMENT_LEN 32

/* Only Linux & PC machines */
#ifndef __LITTLE_ENDIAN__
#define __LITTLE_ENDIAN__
#endif
struct mtt_header_swtimestamp_t {
	union {
		struct {
			unsigned lsw:32;
			unsigned msw:32;
		} t32;

		uint64_t t64;
	} u_ts;
};

/*
* This is the preamble of a trace frame
* is is always 20 bytes, so it can be easily
* scanned for.
*/
typedef struct {
	uint32_t sync_ver_target;
	uint32_t command_param;
	uint32_t component_id;
	uint32_t trace_id;
	uint32_t type_info;
} mtt_protocol_preamble_t;

#define MTT_PTCL_PREAMBLE_LEN	(sizeof(mtt_protocol_preamble_t))
#define MTT_HINT_LENGTH	12 /* For now, a hint is a 12 chars string */

/* Don't use bit fields, as it is the best way to
 * get confused with endless endianness stories...
 *
 * if the media (datalink layer) scrambles the
 * data, then it is a problem of the media driver/decoder,
 * not the problem of the application layer !
 * please refer to ISO/OSI model for better understanding.
 *
 */
typedef struct {
	union {
		/* just for convenience, for real you will allocate
		 * a buffer anyway, and not use this structure
		 * (just cast the real buffer into uint32_t and access
		 * with buf[x] = ...
		 */
		uint64_t raw64[5];
		uint32_t raw32[10];
	} u;
} mtt_protocol_header_t;

#define MTT_SYNC 0xEEE00000 /* Resync corrupt frames, check ptcl version... */

/* Define MTT command parameters
* this is static stuff, initialized one, and reused, low intrusiveness
* In fact the meaning fo PARAM is command specific...
*/

/* Common packet parameters */
#define MTT_PARAM_NACK	0x8000	/* Needs a reply/acknowledgment */
#define MTT_PARAM_ASYNC	0x4000	/* Don't sleep until reply. */
#define MTT_PARAM_RET	0x2000	/* Reply or a notif. (may have not retv) */
#define MTT_PARAM_RETV  0x1000  /* Indicate a return value before the payload*/
#define MTT_PARAM_CRC   0x0800  /* CCITT 16bits CRC (for validation) */

/* parameters for FILE packets */
#define MTT_PARAM_APPEND 0x0001 /* append content if file exists */
#define MTT_PARAM_LAST	0x0002  /* last buffer on this file ID (close) */

/* parameters for TRACE packets */
#define MTT_PARAM_CTXT	0x0001	/* frame has a contextID field */
#define MTT_PARAM_TS64	0x0002	/* frame has a 64 bits timestamp */
#define MTT_PARAM_LEVEL	0x0004	/* frame has level info */

#define MTT_PARAM_MLOC	0x0008	/* Deperecated define. */
#define MTT_PARAM_HINT	MTT_PARAM_MLOC /* This frames uses a hint.*/

#define MTT_PARAM_OST	0x0010	/* TraceId/location is a OST dictionary key */
#define MTT_PARAM_TSTV	0x0020	/* Timestamp as: tv.sec tv.usec */
#define MTT_PARAM_DICT	MTT_PARAM_OST /* Genuine MTT dictionaries. */

typedef uint32_t mtt_parameter_flags_t;

/* Define MTT Target types*/
typedef enum {
	MTT_TARGET_LIN0 = 0x00,	/*CPU0 */
	MTT_TARGET_LIN1 = 0x01,	/*CPU1 */
	MTT_TARGET_ST40 = 0x40,	/*OS21/RT core ST40 */
	MTT_TARGET_ST231 = 0x48, /*OS21/RT core ST200 */
	MTT_TARGET_XP70 = 0x80,	/*XP70 */

	/* Packet send by control interface */
	MTT_TARGET_CTL = 0xFF
} mtt_target_type_t;

/* Value in 32 bits at the right offset to initialize
 * the word. the value is built as follows:
 * cccc_pppp : cccc are the nibble of the command ID and
 * pppp are zero here because this is where the parameter flags go.
 * up to 2^16 commands can be described.
 *
 * value construction rule:
 * ------------------------
 * odd values are for GET accessors, even values are for SET accessors.
 * if we keep this, we can assume that bit16 means a read/not-write op.
 */
typedef enum {
	MTT_CMD_TRACE        = 0x00000000, /* Regular trace command */
	MTT_CMD_META         = 0x40000000, /* Meta data frame */
	MTT_CMD_RAW          = 0x80000000, /* Raw trace- dump */
	MTT_CMD_START        = 0x01000000, /* Start trace session */
	MTT_CMD_STOP         = 0x02000000, /* Stop trace session */
	MTT_CMD_SET_CONF     = 0x03000000, /* Pass configuration info */
	MTT_CMD_VERSION      = 0x04000000, /* Library version */
	MTT_CMD_GET_CNAME    = 0x05000000, /* Notification of new component */
	MTT_CMD_ENTRY_FCT    = 0x06000000, /* Function I/O (needs review) */
	MTT_CMD_EXIT_FCT     = 0x07000000, /* Function I/O (needs review) */
	MTT_CMD_EXTENTRY_FCT = 0x06100000, /* Ext Fc I (with 4 args) */
	MTT_CMD_EXTEXIT_FCT  = 0x07100000, /* Ext Fc O (with return value) */
	MTT_CMD_GET_CONF     = 0x08000000, /* Request configuration info */
	MTT_CMD_CSWITCH      = 0x09000000, /* Generic context switch record. */
	MTT_CMD_GET_CAPS     = 0x0A000000, /* Retrieve capabilities. */
	MTT_CMD_FOPEN     = 0x0B000000, /* Send path, recv status+id. */
	MTT_CMD_FWRITE    = 0x0C000000, /* Send buffer. */
	MTT_CMD_FREAD     = 0x0D000000, /* Request buffer. */
	MTT_CMD_FCLOSE	  = 0x0E000000, /* close file. */

/* Create new get/set commands with the template below.
*  Even is Set, Odd is Get */
	/* _mtt_enum (enumeration API for control tools) */
	MTT_CMD_ENUM = 0x00010000,

	MTT_CMD_SET_COMP_FILT = 0x00020000, /* mtt_get_component_filter */
	MTT_CMD_GET_COMP_FILT = 0x00030000, /* mtt_set_component_filter */
	MTT_CMD_SET_CORE_FILT = 0x00040000, /* mtt_get_core_filter */
	MTT_CMD_GET_CORE_FILT = 0x00050000, /* mtt_set_core_filter */
	MTT_CMD_GET_CLIST = 0x00070000 /* mtt_get_component_list */
} mtt_trace_command_type_t;

/* Deprecated */
#define MTT_CMD_SET_CFILT MTT_CMD_SET_COMP_FILT
#define MTT_CMD_GET_CFILT MTT_CMD_GET_COMP_FILT
#define MTT_CMD_SET_GFILT MTT_CMD_SET_CORE_FILT
#define MTT_CMD_GET_GFILT MTT_CMD_GET_CORE_FILT

#define MTT_PROTOCOL_SET_HEADER(X, target, command, param) { \
(X).u.raw32[0] = MTT_SYNC \
		 |(MTT_PTCL_VER_MAJOR<<12) \
		 |(MTT_PTCL_VER_MINOR<<8) \
		 |(target); \
(X).u.raw32[1] = command | (param & 0x0000FFFF);\
}

#define MTT_PTCL_SET_HEADER(pX, target, command, param) { \
((uint32_t *)(pX))[0] = MTT_SYNC \
		 |(MTT_PTCL_VER_MAJOR<<12) \
		 |(MTT_PTCL_VER_MINOR<<8) \
		 |(target); \
((uint32_t *)(pX))[1] = command | (param & 0x0000FFFF);\
}

/*MTT commands*/
#include "mtt_ptcl_cmds.h"

#ifdef __cplusplus
}
#endif
#endif /*__mtt_PTCL_h*/
