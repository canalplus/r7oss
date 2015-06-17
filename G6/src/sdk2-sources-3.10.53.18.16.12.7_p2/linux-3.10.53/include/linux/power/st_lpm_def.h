/*
 * Copyright (C) 2014 STMicroelectronics Limited
 *
 * Contributor:Francesco Virlinzi <francesco.virlinzi@st.com>
 * Author:Pooja Agarwal <pooja.agarwal@st.com>
 * Author:Udit Kumar <udit-dlh.kumar@st.com>
 * Author:Sudeep Biswas <sudeep.biswas@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public License.
 * See linux/COPYING for more information.
 */

#ifndef __LPM_DEF_H_
#define __LPM_DEF_H_
/*
 * LPM protocol has following architecture
 * |	byte0	|	byte1	|	byte2	|	byte3	|
 * |	cmd id	|	transid	|	msgdata	|	msgdata	|
 *
 * cmd id is command id
 * transid is Transaction ID
 * msgdata is data part of message
 * msg data size can vary depending upon command
 *
 * In case of internal SBC, communication will be done using mailbox.
 * mailbox has depth of 16 bytes.
 * When message is greater than 16 bytes, such messages are treated as big
 * messages and will be send by directly writing into DMEM of SBC.
 *
 * In case of external SBC (7108), which is connected with SOC via i2c
 * communication will be done using i2c bus.
 *
 * Internal and external standby controllers expect message in
 * little endian mode
 */

/* Message command ids */

/* No operation */
#define LPM_MSG_NOP		0x0
#define LPM_MSG_NOP_SIZE	0x0

/* Command id to retrieve version number */
#define LPM_MSG_VER		0x1
#define LPM_MSG_VER_SIZE	0x5

/* Command id to read current RTC value */
#define LPM_MSG_READ_RTC	0x3
#define LPM_MSG_READ_RTC_SIZE	1

/* Command id to trim RTC */
#define LPM_MSG_SET_TRIM	0x4
#define LPM_MSG_SET_TRIM_SIZE	0x0

/* Command id to enter in passive standby mode */
#define LPM_MSG_ENTER_PASSIVE	0x5
#define LPM_MSG_ENTER_PASSIVE_SIZE	0x0

/* Command id  to set watch dog timeout of SBC */
#define LPM_MSG_SET_WDT		0x6
#define LPM_MSG_SET_WDT_SIZE	2

/* Command id  to set new RTC value for SBC */
#define LPM_MSG_SET_RTC		0x7
#define LPM_MSG_SET_RTC_SIZE	6

/* Command id  to configure frontpanel display */
#define LPM_MSG_SET_FP		0x8
#define LPM_MSG_SET_FP_SIZE	1

/* Command id to set wakeup time */
#define LPM_MSG_SET_TIMER	0x9
#define LPM_MSG_SET_TIMER_SIZE	4

/* Command id to get status of SBC CPU */
#define LPM_MSG_GET_STATUS	0xA
#define LPM_MSG_GET_STATUS_SIZE	0x1

/* Command id to generate reset */
#define LPM_MSG_GEN_RESET	0xB
#define LPM_MSG_GEN_RESET_SIZE	0x1

/* Command id to set wakeup device */
#define LPM_MSG_SET_WUD		0xC
#define LPM_MSG_SET_WUD_SIZE	0x0

/* Command id to get wakeup device */
#define LPM_MSG_GET_WUD		0xD
#define LPM_MSG_GET_WUD_SIZE	0x0

/* Command id to offset in SBC memory */
#define LPM_MSG_LGWR_OFFSET	0x10
#define LPM_MSG_LGWR_OFFSET_SIZE	0x2

/* Command id to inform PIO setting */
#define LPM_MSG_SET_PIO		0x11
#define LPM_MSG_SET_PIO_SIZE	14

/* Command id to get advance features */
#define LPM_MSG_GET_ADV_FEA	0x12
#define LPM_MSG_GET_ADV_FEA_SIZE	1

/* Command id to set advance features */
#define LPM_MSG_SET_ADV_FEA	0x13
#define LPM_MSG_SET_ADV_FEA_SIZE	4

/* Command id to set key scan data */
#define LPM_MSG_SET_KEY_SCAN	0x14
#define LPM_MSG_SET_KEY_SCAN_SIZE	2

/*command id for CEC addresses*/
#define LPM_MSG_CEC_ADDR	0x15
#define LPM_MSG_CEC_ADDR_SIZE	4

/*command id for CEC params */
#define LPM_MSG_CEC_PARAMS	0x16
#define LPM_MSG_CEC_PARAMS_SIZE	14

/*command id for CEC Set OSD Name */
#define LPM_MSG_CEC_SET_OSD_NAME  0x19
#define LPM_MSG_CEC_SET_OSD_NAME_SIZE  14

#define LPM_MSG_INFORM_HOST 0x17

/*
 * Command id to set IR information on SBC CPU,
 * these are IR keys on which SBC will do wakeup.
 */
#define LPM_MSG_SET_IR		0x41
#define LPM_MSG_SET_IR_SIZE	0x0

/* Command id to get data associated with some wakeup device */
#define LPM_MSG_GET_IRQ		0x42
#define LPM_MSG_GET_IRQ_SIZE	0x3

/*
 * Command id to inform trace data of SBC,
 * SBC can send trace data to host using this command
 */
#define LPM_MSG_TRACE_DATA	0x43
#define LPM_MSG_TRACE_DATA_SIZE	0x0

/* Command id to read message from SBC memory */
#define LPM_MSG_BKBD_READ	0x44
#define LPM_MSG_BKBD_READ_SIZE	0x0

/* Command id inform SBC that write to SBC memory is done */
#define LPM_MSG_BKBD_WRITE	0x45
#define LPM_MSG_BKBD_WRITE_SIZE	0x6

/* Command id inform SBC that write to SBC memory is done */
#define LPM_MSG_EDID_INFO	0x46
#define LPM_MSG_EDID_INFO_SIZE	0x6

/* Bit-7 of command id used to mark reply from other CPU */
#define LPM_MSG_REPLY		0x80
#define LPM_MSG_REPLY_SIZE	0x0

/* Command for error */
#define LPM_MSG_ERR		0x82
#define LPM_MSG_ERR_SIZE	0x0

/*
 * Version number of driver , this has following fields
 * protocol major and minor number
 * software major, minor and patch number
 * software release build, month, day and year
 */
#define LPM_MAJOR_PROTO_VER	1
#define LPM_MINOR_PROTO_VER	7
#define LPM_MAJOR_SOFT_VER	2
#define LPM_MINOR_SOFT_VER	1
#define LPM_PATCH_SOFT_VER      0
#define LPM_BUILD_MONTH         07
#define LPM_BUILD_DAY           31
#define LPM_BUILD_YEAR          14

/* Used to mask a byte */
#define BYTE_MASK		0xFF

/* For FP setting, mask to get owner's 2 bits */
#define OWNER_MASK		0x3

/* For FP setting, mask to brightness of LED 's 4 bits */
#define NIBBLE_MASK		0xF

/**
 * Mask for PIO level, interrupt and  direction
 * Bit 7 is used for level
 * bit 6 is used for interrupt and
 * bit 5 is used for direction of PIO
 */
#define PIO_LEVEL_SHIFT		7
#define PIO_IT_SHIFT		6
#define PIO_DIRECTION_SHIFT	5

/* Message send to SBC does not have msg data */
#define MSG_ZERO_SIZE		0

/* Transaction id will be generated by lpm itself */
#define MSG_ID_AUTO		0

/* Bit 6 of lpm_config_1 SBC register is to manage IRB enable */
#define IRB_ENABLE		BIT(6)
#define LPM_CONFIG_1_OFFSET	0x4

#define LPM_POWER_STATUS 0x48
#define XP70_IDLE_MODE 0xF

#endif /*__LPM_DEF_H*/

