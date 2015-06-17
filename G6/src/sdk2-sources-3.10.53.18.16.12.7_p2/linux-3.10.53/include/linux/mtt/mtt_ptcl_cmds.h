/*
 *  Multi-Target Trace solution
 *
 *  MTT - PROTOCOL COMMANDS HEADER FILE.
 *
 * internal information for implementation code, not for client code.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) STMicroelectronics, 2011
 */

#ifndef _MTT_PTCL_CMDS_H_
#define _MTT_PTCL_CMDS_H_

/* ================== MTT_CMD_CONFIG =======================
 **/
#define MAX_OUTPUT_DIR_SZ 256
#define MAX_BASENAME_SZ 64
#define MAX_CONFFILE_PATH_SZ 256

/* Data leaves the kernel through a ring buffer (accessed in debugfs)*/
#define MTT_DRV_GUID_DFS \
	(('d' << 0) + ('f' << 8) + ('s' << 16) + ('\0' << 24))

/* Kptrace over ethernet is still relayfs for the kernel. */
#define MTT_DRV_GUID_UDP \
	(('u' << 0) + ('d' << 8) + ('p' << 16) + ('\0' << 24))

/* Data leaves the kernel through the System Trace Module*/
#define  MTT_DRV_GUID_STM \
	(('s' << 0) + ('t' << 8) + ('m' << 16) + ('\0' << 24))

/* Linux trace session parameters (from GUI FRONTEND)*/
struct mtt_linux_cfg {
	uint32_t verbose;
	uint32_t duration; /* The duration of the session (seconds).*/

	/*target-side path when transmitted; keep short */
	char output_basename[MAX_BASENAME_SZ];
	char output_directory[MAX_OUTPUT_DIR_SZ];
	char config_file_path[MAX_CONFFILE_PATH_SZ];
};

struct mtt_os21_cfg {
	uint32_t verbose;
	uint32_t duration;	/* The duration of the session (seconds).*/
};

struct mtt_xp70_cfg {
	uint32_t verbose;
	uint32_t duration;	/* The duration of the session (seconds).*/
};

struct mtt_dfs_drv_cfg {
	/*uses relayfs */
	uint32_t buf_size;
	uint32_t n_subbufs;
};

struct mtt_stm_drv_cfg {
	/*uses STM */
	uint32_t freq_param;
	uint32_t todo;
};

/*
*  ----------- MTT_CMD_CONF --------------------
*  System config structure (kernel side)
*/
#define MTT_STATE_IDLE	0x00 /* No config pending. */
#define MTT_STATE_READY 0x01 /* Configured, ready to start. */
#define MTT_STATE_RUNNING 0x02

typedef struct mtt_sys_kconfig {
	uint32_t session_id;
	uint32_t filter; /* Global filter */
	uint32_t params; /* Session parameters */
	uint32_t state;	/* As #defined above.*/

	/*media specific part */
	uint32_t media_guid;
	union {
		struct mtt_stm_drv_cfg stm;
		struct mtt_dfs_drv_cfg dfs;
	} media_cfg;

	union {
		struct mtt_linux_cfg lin;
		struct mtt_xp70_cfg xp70;
		struct mtt_os21_cfg os21;
	} impl_cfg;

} mtt_sys_kconfig_t;

/*
* ----------- MTT_CMD_VERSION ----------------
* mtt Version declaration notification
* we just send the API version info, as the protocol
* version in the preamble already.
*/
typedef mtt_impl_version_info_t mtt_cmd_version_t;

/*
*  ----------- MTT_CMD_GET_CNAME ----------------
*  component ID declaration notification
*/
#define MTT_PTCL_COMP_NAME_STRING_LEN 64

typedef struct mtt_cmd_get_cname {
	uint32_t comp_id;
	char comp_name[MTT_PTCL_COMP_NAME_STRING_LEN];
} mtt_cmd_get_cname_t;

/*
*  ----------- MTT_CMD_GET_CFILT ----------------
*  component filter change notification
*/
typedef struct mtt_cmd_get_cfilt {
	uint32_t comp_id;
	uint32_t comp_filter;
} mtt_cmd_get_cfilt_t;

/*
* A more thorough getter for component info.
* this should be consistent with info retrieved on
* hot attach (enumeration sequence).
*/
struct mtt_cmd_comp_info {
	uint32_t id;
	uint32_t filter;
	char name[MTT_PTCL_COMP_NAME_STRING_LEN];
};

/*
*  ----------- MTT_CMD_GET/SET_CORE_FILT ----------------
*  component filter change notification
*/
typedef struct mtt_cmd_core_filt {
	uint32_t core_id; /* Consistent with the targetID in MTT. */
	uint32_t core_filter;
} mtt_cmd_core_filt_t;

/*
* Core description structure.
* this should be consistent with info retrieved on
* hot attach (enumeration sequence).
*/

#define MTT_PTCL_CORE_NAME_STRING_LEN 16

struct mtt_cmd_core_info {
	uint32_t id;
	uint32_t filter;
	char name[16]; /*4*4 words in Debug Area*/
};

/*
*  ----------- MTT_CMD_CSWITCH ----------------
*  Generic context switch record.
*  Linux: enforced
*  FreeRTOS: enforced
*  Following Linux kernel coding rules, no new typedef.
*/
struct mtt_cmd_cswitch {
	uint32_t new_ctxt;
};

/*
*  ----------- MTT_CMD_ENTRY/EXIT FCT ----------------
*  Generic Function IO record.
*/
typedef struct mtt_iofunc {
	uint32_t callsite;
} mtt_iofunc_t;

/*
*  ----------- MTT_CMD_EXTENDED_ENTRY FCT ----------------
*  Extended Function IO record.
*/
typedef struct mtt_extended_iofunc_in {
	uint32_t callsite;
	uint32_t arg1;
	uint32_t arg2;
	uint32_t arg3;
	uint32_t arg4;
} mtt_extended_iofunc_in_t;

/*
*  ----------- MTT_CMD_EXTENDED_EXIT FCT ----------------
*  Extended Function IO record.
*/
typedef struct mtt_extended_iofunc_out {
	uint32_t callsite;
	uint32_t return_value;
} mtt_extended_iofunc_out_t;

/* Standard MTT trace packet management structure.
*  Linux: enforced
*/
typedef struct mtt_packet {
	union {
		char *buf;
		mtt_protocol_preamble_t *preamble;
	} u;
	uint32_t length;
	void *comp;
} mtt_packet_t;

#define MTT_KPT_BITOFF_INUSE 0
#define MAX_PKT_SIZE 0x800
#define MAX_PKT_PAYLOAD (MAX_PKT_SIZE-MTT_PTCL_PREAMBLE_LEN)
#define _PKT_SET_CMD(p, c) ((p)->u.preamble->command_param = (c))
#define _PKT_GET_CMD(p)   ((p)->u.preamble->command_param)
#define _PKT_GET_TYPE_INFO(p) ((p)->u.preamble->type_info)

/* The macro below initializes the packet lenght,
 * you still have to add the size for any optional fields.
 */
#define _PKT_SET_LEN(p, t) { \
	(p)->u.preamble->type_info = (t); \
	(p)->length = (sizeof(mtt_protocol_preamble_t) + ((t) & 0x0000FFFF)); \
}

#define _PKT_IS_MTT(p) \
	(((p)->u.preamble->sync_ver_target & 0xFFF00000) == MTT_SYNC)
#define _PKT_HAS_ACK(p) ((p)->u.preamble->command_param & MTT_PARAM_NACK)
#define _PKT_HAS_RET(p) ((p)->u.preamble->command_param & MTT_PARAM_RET)
#define _PKT_HAS_ASYNC(p) ((p)->u.preamble->command_param & MTT_PARAM_ASYNC)

#define _PKT_PAYLOAD(p) ((char *)((p)->u.buf)+sizeof(mtt_protocol_preamble_t))

#endif /*_MTT_PTCL_CMDS_H_*/
