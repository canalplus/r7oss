/************************************************************************
Copyright (C) 2012 STMicroelectronics. All Rights Reserved.

This file is part of the stm_fe Library.

stm_fe is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

stm_fe is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with stm_fe; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The stm_fe Library may alternatively be licensed under a proprietary
license from ST.

Source file name : d0remote.h
Author :           Shobhit

Header for Remote PCPD LLA

Date        Modification                                    Name
----        ------------                                    --------
1-Nov-12   Created                                         Shobhit

************************************************************************/
#ifndef _D0REMOTE_H
#define _D0REMOTE_H

/* *1442* - 1768- PCPD actual pay load size */
#define PCPD_BUFFER_DATA_SIZE		1530
#define PCPD_HEADER_SIZE		16 /* PCPD Packet header size */
/*1442+16  1488Maximum IP payload 1472 + PCPD Header Length 16 */
#define MAXRCVPKTLEN			1458
#define PCPD_CM_VERSION			2
#define MAGIC_NUM			0x1EE7
#define CPUID_ESTB			0
#define CPUID_ECM			3
#define ROLEID_ESTB			17
#define ROLEID_ECM			0
#define ROLEID_BROADCAST		0xFF
#define PCPD_COMMAND_TYPE		3/* Control Interface message type */
#define PCPD_REMOTE_TUNER_REQUEST	0x0226
#define PCPD_REMOTE_TUNER_RESPONSE	0x0227
#define MAX_MSG_SIZE			256
#define FIFO_SIZE			128
#define ESTB_PORT_SYNC_CMD		47122
#define ECM_PORT_SYNC_CMD		30120
/*#define ESTB_IP_ADDR			0x0ac78473*/
#define ECM_IP_ADDR			0xc0a86401 /*"192.168.100.1"*/
/*Remote Tuner Command*/
#define STPCPD_REMOTE_OPEN			0
#define STPCPD_REMOTE_STANDBY			2
#define STPCPD_REMOTE_SEARCH			3
#define STPCPD_REMOTE_TRACKING			4
#define STPCPD_REMOTE_CLOSE			1
#define	STPCPD_REMOTE_STATUS_SUCCESS		0
#define	STPCPD_REMOTE_STATUS_HANDLE_KO		1
#define	STPCPD_REMOTE_STATUS_CHANNELID_KO	2
#define	STPCPD_REMOTE_STATUS_PARAMETER_KO	3
#define	STPCPD_REMOTE_STATUS_LOCKED_KO		4
#define	STPCPD_REMOTE_STATUS_STANDBY_OK		5
/* fe_wait_event_interruptible_timeout delay used */
#define REMOTE_DELAY_MS			10000

struct pcpd_packet_data_t {
	u_int8_t version;		/* version of PCPD */
	u_int8_t type;/*Type pf message 0-Unknown,1-DCD,2- Data PDU,3-Command*/
	u_int16_t length:12;	/*Length of PCPD packet excluding PCPD Header*/
	u_int16_t offset:4;/* Byte offset from where actual data starts */
	u_int16_t destination;	/*Destination */
	u_int16_t source;		/*Source */
	u_int16_t txn_id;/* Used for comand packets that require response */
	u_int16_t magic_number;	/*Magic Number */
	u_int32_t cmd_id;		/* Command ID for command packet */
	u_int8_t data[PCPD_BUFFER_DATA_SIZE];	/*Actual payload */
} __packed;

enum stpcpd_remote_ts_config_e {
	STPCPD_REMOTE_TS_SERIAL_PUNCT_CLOCK = (1 << 0),
	STPCPD_REMOTE_TS_SERIAL_CONT_CLOCK = (1 << 1),
	STPCPD_REMOTE_TS_PARALLEL_PUNCT_CLOCK = (1 << 2),
	STPCPD_REMOTE_TS_DVBCI_CLOCK = (1 << 3),
	STPCPD_REMOTE_TS_FALLINGEDGE_CLOCK = (1 << 4),
	STPCPD_REMOTE_TS_SYNCBYTE_OFF = (1 << 5),
	STPCPD_REMOTE_TS_PARITYBYTES_ON = (1 << 6),
	STPCPD_REMOTE_TS_SWAP_ON = (1 << 7),
	STPCPD_REMOTE_TS_SMOOTHER_ON = (1 << 8)
};

enum stpcpd_remote_modulation_e {
	STPCPD_REMOTE_MOD_NONE   = 0x00,  /* Modulation unknown */
	STPCPD_REMOTE_MOD_QAM    = (1 << 2),
	STPCPD_REMOTE_MOD_4QAM   = (1 << 3),
	STPCPD_REMOTE_MOD_16QAM  = (1 << 4),
	STPCPD_REMOTE_MOD_32QAM  = (1 << 5),
	STPCPD_REMOTE_MOD_64QAM  = (1 << 6),
	STPCPD_REMOTE_MOD_128QAM = (1 << 7),
	STPCPD_REMOTE_MOD_256QAM = (1 << 8)
};

enum stpcpd_fec_type_e {
	STPCPD_REMOTE_ANNEX_A_C	= 1,
	STPCPD_REMOTE_ANNEX_B	= 2
};

void store_data_le32(u_int8_t *msg, u_int32_t value);
u_int16_t load_data_le16(u_int8_t *msg);
unsigned int load_data_le32(u_int8_t *msg);
void pcpd_create_msg(struct stm_fe_demod_s *priv,
					struct pcpd_packet_data_t *pcpd_msg);
int transport_setup(struct stm_fe_demod_s *priv);
u_int32_t remote_modulationmap(stm_fe_demod_modulation_t mod);
stm_fe_demod_modulation_t remote_getmodulation(u_int32_t mod);

int remote_init(struct stm_fe_demod_s *priv);
int remote_tune(struct stm_fe_demod_s *priv, bool *lock);
int remote_status(struct stm_fe_demod_s *priv, bool *locked);
int remote_tracking(struct stm_fe_demod_s *priv);
int stm_fe_remote_attach(struct stm_fe_demod_s *priv);
int stm_fe_remote_detach(struct stm_fe_demod_s *priv);
int remote_standby(struct stm_fe_demod_s *priv, bool standby);
int remote_restore(struct stm_fe_demod_s *priv);
int remote_abort(struct stm_fe_demod_s *priv, bool abort);
int remote_unlock(struct stm_fe_demod_s *priv);
int remote_term(struct stm_fe_demod_s *priv);

#endif /* _D0REMOTE_H */
