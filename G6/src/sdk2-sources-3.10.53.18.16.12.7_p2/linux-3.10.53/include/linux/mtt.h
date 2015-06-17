/*
Copyright (C) 2010, 2011,2012 STMicroelectronics. All Rights Reserved.

This file is part of the MTT Library.

MTT is free software; you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License version 2.1 as published by the
Free Software Foundation.

MTT is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with MTT; see the file COPYING.
If not, write to the Free Software Foundation,
59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

Written by Serge DE PAOLI, July 2012. Contact serge.de-paoli@st.com

The MTT Library may alternatively be licensed under a proprietary
license from ST.
*/


#ifndef _MTT_API_H_
#define _MTT_API_H_

#ifndef __KERNEL__
#include "mtt_types.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define MTT_API_VER_MAJOR	1
#define MTT_API_VER_MINOR	2
#define MTT_API_VERSION	        ((MTT_API_VER_MAJOR<<16)|MTT_API_VER_MINOR)
#define MTT_API_VER_STRING      "MTT API v1.02 build-"__DATE__
#define MTT_API_VER_STRING_LEN  32

typedef enum {
	MTT_ERR_NONE = 0x0000,
	MTT_ERR_OTHER = 0xffff,
	MTT_ERR_FN_UNIMPLEMENTED = 0x0100,
	MTT_ERR_USAGE = 0x0101,
	MTT_ERR_PARAM = 0x0102,
	MTT_ERR_CONNECTION = 0x0103,
	MTT_ERR_ALREADY_CNF = 0x0104,
	MTT_ERR_TIMEOUT = 0x0106,
	MTT_ERR_FERROR = 0x0107,
	MTT_ERR_SOCKERR = 0x0108,
	MTT_ERR_ALREADY_OPENED = 0x0109
} mtt_return_t;

#define MTT_CAPS_LOCAL_TIME	0x10
#define MTT_CAPS_DROP_RECORDS	0x02
#define MTT_CAPS_LOG_LEVEL	0x04
#define MTT_CAPS_CONTEXT	0x08

#define MTT_CAPS_STMV1		0x1000 /* V1 and V2 */
#define MTT_CAPS_STMV3		0x2000
#define MTT_CAPS_STM500		0x4000 /* early stage */

typedef enum {
	MTT_INIT_LOCAL_TIME = MTT_CAPS_LOCAL_TIME,
	MTT_INIT_DROP_RECORDS = MTT_CAPS_DROP_RECORDS,
	MTT_INIT_LOG_LEVEL = MTT_CAPS_LOG_LEVEL,
	MTT_INIT_CONTEXT = MTT_CAPS_CONTEXT
} mtt_init_attr_bit_t;

typedef struct {
	mtt_init_attr_bit_t attr;
	char name[64];
	uint32_t reserved;
} mtt_init_param_t;

typedef struct {
	uint32_t version;
	char string[MTT_API_VER_STRING_LEN];
} mtt_impl_version_info_t;

typedef struct {
	mtt_impl_version_info_t impl_info;
	uint32_t caps;	/* compatible with mtt_init_attr_bit_t
			 * to pass to mtt_initialize */
	uint32_t caps2;
} mtt_api_caps_t;

extern mtt_init_param_t mtt_init_params;

typedef void *mtt_comp_handle_t;

#define MTT_COMP_ID_ANY           (-1)
#define MTT_COMP_ID_MASK          0x0FFFFFFF
#define MTT_COMP_ID(my_custom_id) (my_custom_id & MTT_COMP_ID_MASK)

/*
 * Trace Verbosity and filtering options,
 */

typedef uint32_t mtt_trace_level_t;

/* Full trace or mute. */
#define MTT_LEVEL_ALL		0xffffffff
#define MTT_LEVEL_NONE		0x00000000

/* Standard trace levels.*/
#define MTT_LEVEL_ERROR		0x80000000 /* non fatal errors */
#define MTT_LEVEL_WARNING	0x40000000
#define MTT_LEVEL_INFO		0x20000000
#define MTT_LEVEL_DEBUG		0x10000000
#define MTT_LEVEL_ASSERT	0x08000000 /* beyond recovery */

/* When implemented: this flag will cause a callstack frame
 * to be append to the trace payload. Use with caution as this
 * can make the trace point intrusive.
 */
#define MTT_LEVEL_CALLSTACK	0x04000000

/* You may use any bit between USER0 and USERLAST
 * to create your own verbosity mask.
 */
#define MTT_LEVEL_USERMASK	0x0000FFFF
#define MTT_LEVEL_USER0		0x00000001
#define MTT_LEVEL_USER1		0x00000002
#define MTT_LEVEL_USER2		0x00000004
#define MTT_LEVEL_USER3		0x00000008
#define MTT_LEVEL_USERLAST	0x00008000

/* Deprecated, use mask instead to know if there
 * is a user defined level in the mask. */
#define MTT_LEVEL_USERTAG MTT_LEVEL_USERMASK

/* Macro for the user to safely build his own level
 * filter masks. */
#define MTT_LEVEL_USER(X) ((X) & MTT_LEVEL_USERMASK)

/* Cursors: the tools will implement specific markers for cursors,
 * so they can be used to highlight the beginning/end of one or more
 * sequences, and provide cursor-specific viewing/statistics.
 */
#define MTT_LEVEL_CURSORA	0x00010000
#define MTT_LEVEL_CURSORB	0x00020000
#define MTT_LEVEL_CURSORC	0x00040000
#define MTT_LEVEL_CURSORD	0x00080000

/* OS21 uses two cursors to mark entry/exit of function calls
 *
 * Baremachine/Self-backed packet building may use
 * MTT_CMD_FUNC_ENTRY/EXIT packet types, to benefit
 * from the analysis & profiling tools.
 */
#define MTT_LEVEL_API_ENTRY	MTT_LEVEL_CURSORA
#define MTT_LEVEL_API_RETURN	MTT_LEVEL_CURSORB

typedef enum {
	MTT_TRIGACTION_RING = 0x00000001,
	MTT_TRIGACTION_FLUSH = 0x00000002,
	MTT_TRIGACTION_APPLY_FILTER = 0x00000004,
	MTT_TRIGACTION_START_ON = 0x00000008,
	MTT_TRIGACTION_STOP_ON = 0x00000010
} mtt_trigger_action_t;

typedef struct {
	uint32_t component;
	mtt_trace_level_t level;
	uint32_t action;
} mtt_trigger_desc_t;

typedef enum {
	MTT_TYPE_UINT8 = 0x01000000,
	MTT_TYPE_INT8 = 0x02000000,
	MTT_TYPE_UINT16 = 0x03000000,
	MTT_TYPE_INT16 = 0x04000000,
	MTT_TYPE_UINT32 = 0x05000000,
	MTT_TYPE_INT32 = 0x06000000,
	MTT_TYPE_UINT64 = 0x07000000,
	MTT_TYPE_INT64 = 0x08000000,
	MTT_TYPE_FLOAT = 0x09000000,
	MTT_TYPE_DOUBLE = 0x0a000000,
	MTT_TYPE_BOOL = 0x0b000000,
} mtt_type_scalar_t;

typedef enum {
	MTT_TYPEP_SCALAR = 0x00000000,
	MTT_TYPEP_VECTOR = 0x00010000,
	MTT_TYPEP_STRING = 0x00020000,
	MTT_TYPEP_SHOWHEX = 0x00040000,
} mtt_type_param_t;

#define MTT_TYPE(scalar_type, type_param, element_size, nb_elements) \
(((scalar_type|type_param) & 0xFFFF0000) \
+ ((element_size)*(nb_elements) & 0x0000FFFF))

typedef enum {
	MTT_TRACEITEM_UINT8 =
	    MTT_TYPE(MTT_TYPE_UINT8, MTT_TYPEP_SCALAR, 1, 1),
	MTT_TRACEITEM_INT8 =
	    MTT_TYPE(MTT_TYPE_INT8, MTT_TYPEP_SCALAR, 1, 1),
	MTT_TRACEITEM_UINT16 =
	    MTT_TYPE(MTT_TYPE_UINT16, MTT_TYPEP_SCALAR, 2, 1),
	MTT_TRACEITEM_INT16 =
	    MTT_TYPE(MTT_TYPE_INT16, MTT_TYPEP_SCALAR, 2, 1),
	MTT_TRACEITEM_UINT32 =
	    MTT_TYPE(MTT_TYPE_UINT32, MTT_TYPEP_SCALAR, 4, 1),
	MTT_TRACEITEM_INT32 =
	    MTT_TYPE(MTT_TYPE_INT32, MTT_TYPEP_SCALAR, 4, 1),
	MTT_TRACEITEM_UINT64 =
	    MTT_TYPE(MTT_TYPE_UINT64, MTT_TYPEP_SCALAR, 8, 1),
	MTT_TRACEITEM_INT64 =
	    MTT_TYPE(MTT_TYPE_INT64, MTT_TYPEP_SCALAR, 8, 1),
	MTT_TRACEITEM_FLOAT =
	    MTT_TYPE(MTT_TYPE_FLOAT, MTT_TYPEP_SCALAR, 4, 1),
	MTT_TRACEITEM_DOUBLE =
	    MTT_TYPE(MTT_TYPE_DOUBLE, MTT_TYPEP_SCALAR, 8, 1),
	MTT_TRACEITEM_BOOL =
	    MTT_TYPE(MTT_TYPE_BOOL, MTT_TYPEP_SCALAR, 4, 1),
	MTT_TRACEITEM_PTR32 =
	    MTT_TYPE(MTT_TYPE_UINT32, MTT_TYPEP_SHOWHEX, 4, 1)
} mtt_trace_item_t;

#define MTT_TRACEITEM_STRING(size)    \
MTT_TYPE(MTT_TYPE_UINT8,   MTT_TYPEP_STRING, 1, size)

#define MTT_TRACEITEM_BLOB(size)      \
MTT_TYPE(MTT_TYPE_UINT8,   MTT_TYPEP_VECTOR, 1, size)

#define MTT_TRACEITEM_DUMP(size)      \
MTT_TYPE(MTT_TYPE_UINT32,  MTT_TYPEP_VECTOR|MTT_TYPEP_SHOWHEX, 4, size)

#define MTT_TRACEVECTOR_UINT8(size)   \
MTT_TYPE(MTT_TYPE_UINT8,   MTT_TYPEP_VECTOR, 1, size)

#define MTT_TRACEVECTOR_INT8(size)    \
MTT_TYPE(MTT_TYPE_INT8,   MTT_TYPEP_VECTOR, 1, size)

#define MTT_TRACEVECTOR_UINT16(size)  \
MTT_TYPE(MTT_TYPE_UINT16, MTT_TYPEP_VECTOR, 2, size)

#define MTT_TRACEVECTOR_INT16(size)   \
MTT_TYPE(MTT_TYPE_INT16,  MTT_TYPEP_VECTOR, 2, size)

#define MTT_TRACEVECTOR_UINT32(size)  \
MTT_TYPE(MTT_TYPE_UINT32, MTT_TYPEP_VECTOR, 4, size)

#define MTT_TRACEVECTOR_INT32(size)   \
MTT_TYPE(MTT_TYPE_INT32,  MTT_TYPEP_VECTOR, 4, size)

#define MTT_TRACEVECTOR_UINT64(size)  \
MTT_TYPE(MTT_TYPE_UINT64, MTT_TYPEP_VECTOR, 8, size)

#define MTT_TRACEVECTOR_INT64(size)   \
MTT_TYPE(MTT_TYPE_INT64,  MTT_TYPEP_VECTOR, 8, size)

#define MTT_TRACEVECTOR_FLOAT(size)   \
MTT_TYPE(MTT_TYPE_FLOAT,  MTT_TYPEP_VECTOR, 4, size)

#define MTT_TRACEVECTOR_DOUBLE(size)  \
MTT_TYPE(MTT_TYPE_DOUBLE, MTT_TYPEP_VECTOR, 8, size)


#define MTT_COMP_NAME_LEN 64

typedef struct {
	uint32_t port;
	uint32_t id;
	uint32_t level;
	char name[MTT_COMP_NAME_LEN];
	mtt_comp_handle_t handle;
	uint64_t stat[4];
} mtt_comp_info_t;

typedef struct {
	uint32_t nb;
	mtt_comp_info_t *comp_info;
} mtt_comp_info_list_t;


#if !defined(__KERNEL__) || defined(CONFIG_MTT)
mtt_return_t
mtt_initialize(const mtt_init_param_t *init_param);

mtt_return_t
mtt_uninitialize(void);

mtt_return_t
mtt_get_capabilities(mtt_api_caps_t *api_caps);

mtt_return_t
mtt_open(const uint32_t comp_id,
	const char *comp_name, mtt_comp_handle_t *comp_handle);

mtt_return_t
mtt_get_component_handle(const uint32_t comp_id,
	mtt_comp_handle_t *comp_handle);

mtt_return_t
mtt_close(const mtt_comp_handle_t comp_handle);

mtt_return_t
mtt_set_core_filter(const uint32_t new_level,
	uint32_t *prev_level);

mtt_return_t
mtt_get_core_filter(uint32_t *level);

mtt_return_t
mtt_set_component_filter(const mtt_comp_handle_t comp_handle,
	const uint32_t new_level,
	uint32_t *prev_level);

mtt_return_t
mtt_get_component_filter(const mtt_comp_handle_t comp_handle,
	uint32_t *level);

mtt_return_t
mtt_trace(const mtt_comp_handle_t component,
	const mtt_trace_level_t level,
	const uint32_t type_info,
	const void *data, const char *hint);

#define mtt_trace16(co, level, us)	\
	mtt_trace(co, level, MTT_TRACEITEM_UINT16, &us, NULL);

#define mtt_trace32(co, level, us)	\
	mtt_trace(co, level, MTT_TRACEITEM_UINT32, &us, NULL);

mtt_return_t
mtt_get_component_info(const mtt_comp_handle_t comp_handle,
		   mtt_comp_info_t *comp_info);

mtt_return_t
mtt_get_component_info_list(mtt_comp_info_list_t *comp_info_list);

mtt_return_t
mtt_print(const mtt_comp_handle_t component,
	const mtt_trace_level_t level,
	const char *format_string, ...);

mtt_return_t
mtt_data_raw(const uint32_t componentid,
	const uint32_t level,
	const uint32_t length, const void *data);

mtt_return_t
mtt_trigger(const mtt_comp_handle_t component,
	const mtt_trigger_desc_t *trigger,
	const void *data, const uint32_t size);
#else
#define mtt_initialize(a)	do {} while (0);
#define mtt_uninitialize()	do {} while (0);
#define mtt_get_capabilities(a)	do {} while (0);
#define mtt_open(a, b, c)	do {} while (0);
#define mtt_get_component_handle(a, b)	do {} while (0);
#define mtt_close(a)		do {} while (0);
#define mtt_set_core_filter(a, b) do {} while (0);
#define mtt_get_core_filter(a)	do {} while (0);
#define mtt_set_component_filter(a, b, c) do {} while (0);
#define mtt_get_component_filter(a, b)	do {} while (0);
#define mtt_trace(a, b, c, d, e) do {} while (0);
#define mtt_trace16(a, b, c)	do {} while (0);
#define mtt_trace32(a, b, c)	do {} while (0);
#define mtt_get_component_info(a, b)	do {} while (0);
#define mtt_get_component_info_list(a)	do {} while (0);
#define mtt_print(a, b, c, ...)	do {} while (0);
#define mtt_data_raw(a, b, c, d) do {} while (0);
#define mtt_trigger(a, b, c, d)	do {} while (0);
#endif

#ifdef __cplusplus
}
#endif
#endif
