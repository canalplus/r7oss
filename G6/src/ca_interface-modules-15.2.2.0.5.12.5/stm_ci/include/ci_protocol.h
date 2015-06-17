/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

This file is part of the stm_fe	Library.

stm_fe is free software; you can redistribute it and/or	modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

stm_fe is distributed in the hope that it will be useful,
but WITHOUT ANY	WARRANTY; without even the implied warranty of
MERCHANTABILITY	or FITNESS FOR A PARTICULAR PURPOSE.
See the	GNU General Public License for more details.

You should have	received a copy	of the GNU General Public License along
with stm_fe; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,	USA.

The stm_fe Library may alternatively be	licensed under a proprietary
license	from ST.

Source file name : ci_protocol.h
Author :	   Meghag

API dedinitions	for demodulation device

Date	    Modification				    Name
----	    ------------				    --------
05-Aug -12   Created					     Meghag

************************************************************************/
#ifndef	_CI_PROTOCOL_H
#define	_CI_PROTOCOL_H

#include "ca_os_wrapper.h"
#include "linux/sched.h"
#include "ci_internal.h"

/* IO registers	Offset */
#define	CI_DATA		0x0     /* Data register */

/* Status register bits	*/
#define	CI_RE		0x01	 /* Read error */
#define	CI_WE		0x02	/* Write error */
#define	CI_IIR		0x10	 /* Initialize Interface request */
#define	CI_FR		0x40	 /* Free */
#define	CI_DA		0x80	/* Data	available */

/* Command register bits */
#define	CI_SETHC	0x01    /*	Host control bit set */
#define	CI_SETSW	0x02    /* Size write set */
#define	CI_SETSR	0x04    /*	Size read set  */
#define	CI_SETRS	0x08    /*	Interface reset	*/
#define	CI_SETDAIE	0x80	   /* Data available interrupt enable*/
#define	CI_SETFRIE	0x40    /* Module free interrupt enable */

#define	CI_RESET_COMMAND_REG	0x00
#define	CI_CMDSTATUS		0x1     /*	Command/Status register	*/
#define	CI_SIZE_LS		0x2	  /* Size register (LS)	*/
#define	CI_SIZE_MS		0x3	 /* Size register (MS) */

/* EN50221 Specific Constants */
#define	STCE_EV		"DVBCI_HOST"
#define	STCE_PD		"DVB_CI_MODULE"
#define	STCF_LIB	"DVB_CI_V1.00"
#define	TPCE_IO		0x22
#define	TPCE_IF		0x04
#define	MAX_COR_BASE_ADDRESS		0xFFE
#define	STCI_IFN_HIGH			0x02
#define	STCI_IFN_LOW			0x41
#define	IND_TPLLV1_MINOR		0x01
#define	IND_TPLLV1_MAJOR		0x00
#define	VAL_TPLLV1_MINOR		0x00
#define	VAL_TPLLV1_MAJOR		0x05
#define	SZ_TPLLMANFID			0x04
#define	IND_TPLMID_MANF_LSB		0x00
#define	IND_TPLMID_MANF_MSB		0x01
#define	IND_TPLMID_CARD_LSB		0x02
#define	IND_TPLMID_CARD_MSB		0x03

/* PC Card Vol 2 related Constants */
#define	CISTPL_DEVICE			0x01     /*	Tupple Tags */
#define	CISTPL_NO_LINK			0x14
#define	CISTPL_VERS_1			0x15
#define	CISTPL_17			0x17
#define	CISTPL_CONFIG			0x1a
#define	CISTPL_CFTABLE_ENTRY		0x1b
#define	CISTPL_DEVICE_OC		0x1c
#define	CISTPL_DEVICE_OA		0x1d
#define	CISTPL_MANFID			0x20
#define	CISTPL_FUNCID			0x21
#define	CISTPL_END			0xff

#define	NOT_READY_WAIT			0x1F     /*	Represent neither WAIT#	nor READY# asserted */
#define	WAIT_SCALE_0			0x1C     /*	WAIT_SCALE allowable value of WAIT# assertion */
#define	WAIT_SCALE_1			0x1D     /*	depending upon possible	settings of TPCE_TD(0-1) bits*/
#define	WAIT_SCALE_2			0x1E

#define	READY_SCALE_0			0x03     /*	READY_SCALE allowable value of READY# assertion	*/
#define	READY_SCALE_1			0x07     /*	depending upon possible	settings of TPCE_TD(2-4) bits*/
#define	READY_SCALE_2			0x0B
#define	READY_SCALE_3			0x0F
#define	READY_SCALE_4			0x13
#define	READY_SCALE_5			0x17
#define	READY_SCALE_6			0x1b

#define	TPCC_RADR_1_BYTE		0x0	     /*	TPCC_RADR is encoded in	one byte */
#define	TPCC_RADR_2_BYTE		0x1	     /*	TPCC_RADR is encoded in	two byte */
#define	TPCC_RADR_3_BYTE		0x2	     /*	TPCC_RADR is encoded in	thre byte */
#define	TPCC_RADR_4_BYTE		0x3	     /*	TPCC_RADR is encoded in	four byte */

#define	MASK_TPCC_RFSZ			0xC0     /*	Masking	Bit Pattern for	TPCC_RFSZ in TPCC_SZ */
#define	MASK_TPCC_RMSZ			0x3C     /*	Masking	Bit Pattern for	TPCC_RMSZ in TPCC_SZ */
#define	MASK_TPCC_RASZ			0x03     /*	Masking	Bit Pattern for	TPCC_RASZ in TPCC_SZ */

#define	MASK_IO_ADDLINES		0x1f     /*	Masking	Bit Pattern for	IO Tupple */
#define	MASK_TPCE_INDX			0x3F     /*	Masking	Bit Pattern for	Config Index */
#define	MASK_RW_SCALE			0x1F     /*	Masking	Bit Pattern for	Read Write Timing Tupple */
/* Private Constants */
#define	CI_MAX_WRITE_RETRY		0xff     /* Number of retries while writing the last byte	*/
#define	CI_MAX_RESET_RETRY		1000	/* Number of retries while doing CI Reset(This is H/w work around) */
#define	CI_MAX_CHECK_CIS_RETRY		1200	  /* Number of retries while checking CIS(This is H/w work around) MG */
#define	CI_CAM_CHECK_COUNT		50

#define	MAX_NO_OF_TUPLES		0xff
#define	MAX_TUPLE_BUFFER		0xff	    /* Maximum Tupple Buffer Size */
#define	EVEN_ADDRESS_SHIFT		0x01     /*	For accessing the Even Address in Attribute Memory Space */
#define	UPPER_BYTE			0xff00   /*	To extract upper byte off 16 bit data */
#define	LOWER_BYTE			0x00ff   /*	To extract lower byte off 16 bit data */
#define	SHIFT_EIGHT_BIT			0x08     /*	Shift the value	left by	8 bits */
#define	BIT_VALUE_HIGH			0x01     /*	Showing	bit is High */
#define	BIT_VALUE_LOW			0x00     /*	Showing	bit is Low */
#define	MASK_0_BIT			0x01     /*	Masking	bit 0 of byte */
#define	MASK_1_BIT			0x02     /*	Masking	bit 1 of byte */
#define	MASK_2_BIT			0x04     /*	Masking	bit 2 of byte */
#define	MASK_3_BIT			0x08     /*	Masking	bit 3 of byte */
#define	MASK_4_BIT			0x10     /*	Masking	bit 4 of byte */
#define	MASK_5_BIT			0x20     /*	Masking	bit 5 of byte */
#define	MASK_6_BIT			0x40     /*	Masking	bit 6 of byte */
#define	MASK_7_BIT			0x80     /*	Masking	bit 7 of byte */

#define	STATUS_CIS_CHECKED		0x00001111  /* To check	the presence of	all required Tupple */
#define	STATUS_CIS_MANFID_CHECKED	0x00000010	/* To check the	presence of MANFID Tupple */
#define	STATUS_CIS_VERS_1_CHECKED	0x00000001	/* To check the	presence of VERS Tupple	*/
#define	STATUS_CIS_CONFIG_CHECKED	0x00000100	/* To check the	presence of CONFIG Tupple */
#define	STATUS_CIS_CFTENTRY_CHECKED	0x00001000  /* To check the presence of CFENTRY Tupple */


/* CIPLUS related constants*/
#define	TPLLV1_INFO			0x02
#define	CIPLUS_PRODUCT_INFO_START	"$compatible["
#define	CIPLUS_PRODUCT_INFO_END		"]$"
#define	CIPLUS_STRING_IDENTIFIER	"ciplus=1"
#define	CIPLUS_OPERATOR_STRING_IDENTIFIER	"ciprof="

typedef	struct {
    uint8_t  tuple_tag;
    uint8_t  tuple_link;
    uint8_t  tuple_data[MAX_TUPLE_BUFFER];
} ci_tupple_t;

ca_error_code_t  ci_write_command(stm_ci_t	*ci_p, stm_ci_command_t	command,uint8_t	value);
ca_error_code_t ci_read_status(stm_ci_t   *ci_p, stm_ci_status_t  status,uint8_t  *value_p);

void  ci_write_cor(stm_ci_t *ci_p);
ca_error_code_t  ci_write_data(stm_ci_t *ci_p,stm_ci_msg_params_t	*params_p);
ca_error_code_t  ci_read_data (stm_ci_t *ci_p,stm_ci_msg_params_t	*params_p);

void ci_read_status_register(stm_ci_t	*ci_p, uint8_t *status_value_p);
void  ci_write_command_reg(stm_ci_t *ci_p,  uint8_t	value);
ca_error_code_t   ci_configure_card (stm_ci_t *ci_p);

#endif /*_CI_PROTOCOL_H*/
