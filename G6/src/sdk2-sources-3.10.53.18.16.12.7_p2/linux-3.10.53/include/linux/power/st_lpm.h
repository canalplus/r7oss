/*
 * Interface file for st lpm driver
 *
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

#ifndef __LPM_H
#define __LPM_H

#include <linux/rtc.h>
#include "st_lpm_def.h"

/**
 * enum st_lpm_wakeup_devices- define wakeup devices
 * One bit for each wakeup device
 */
enum st_lpm_wakeup_devices {
	ST_LPM_WAKEUP_IR	=	BIT(0),
	ST_LPM_WAKEUP_CEC	=	BIT(1),
	ST_LPM_WAKEUP_FRP	=	BIT(2),
	ST_LPM_WAKEUP_WOL	=	BIT(3),
	ST_LPM_WAKEUP_RTC	=	BIT(4),
	ST_LPM_WAKEUP_ASC	=	BIT(5),
	ST_LPM_WAKEUP_NMI	=	BIT(6),
	ST_LPM_WAKEUP_HPD	=	BIT(7),
	ST_LPM_WAKEUP_PIO	=	BIT(8),
	ST_LPM_WAKEUP_EXT	=	BIT(9),
	ST_LPM_WAKEUP_MANUAL_RST =	BIT(10),
	ST_LPM_WAKEUP_SW_WDT_RST =	BIT(11),
	ST_LPM_WAKEUP_CUST	=	BIT(15)
};

struct st_lpm_wkup_dev_name {
	unsigned int index;
	char name[5];
};

struct st_sbc_platform_data {
	unsigned gpio;
	unsigned trigger_level;
};

/**
 * enum st_lpm_reset_type - define reset type
 * @ST_LPM_SOC_RESET:	SOC reset
 * @ST_LPM_SBC_RESET:	Only SBC reset
 * @ST_LPM_BOOT_RESET:	reset SBC and stay in bootloader
 */
enum st_lpm_reset_type {
	ST_LPM_SOC_RESET,
	ST_LPM_SBC_RESET = BIT(0),
	ST_LPM_BOOT_RESET = BIT(1),
	ST_LPM_HOST_RESET = BIT(2)
};

/**
 * enum st_lpm_sbc_state - defines SBC state
 * @ST_LPM_SBC_BOOT:	SBC waiting in bootloader
 * @ST_LPM_SBC_RUNNING:	SBC is running
 * @ST_LPM_SBC_STANDBY:	Entering into standby
 */
enum st_lpm_sbc_state {
	ST_LPM_SBC_BOOT		=	1,
	ST_LPM_SBC_RUNNING	=	4,
	ST_LPM_SBC_STANDBY	=	5
};

/**
 * struct st_lpm_version - define version information
 * @major_comm_protocol:	Supported Major protocol version
 * @minor_comm_protocol:	Supported Minor protocol version
 * @major_soft:			Major software version
 * @minor_soft:			Minor software version
 * @patch_soft:			Software patch version
 * @month:			Software build month version
 * @day:			Software build day
 * @year:			Software build year
 * @custom_id:			Used for customizing fw by FAE
 *
 * Same struct is used for firmware and driver version information
 */
struct st_lpm_version {
	char major_comm_protocol;
	char minor_comm_protocol;
	char major_soft;
	char minor_soft;
	char patch_soft;
	char month;
	char day;
	char year;
	char custom_id;
};

/**
 * struct st_lpm_fp_setting - define front panel setting
 * @owner:	Owner of front panel
 *		when 0 - SBC firmware will be owner in standby
 *		when 1 - SBC firmware always own frontpanel display
 *		when 2 - Host will always own front panel
 * @am_pm:	AM/PM indicator, when 0 clock will be displayed in 24 hrs format
 * @brightness:	brightness of display, [0-3] bits are used max value is 15
 * This is to inform SBC how front panel display will be used
 */
struct st_lpm_fp_setting {
	char owner;
	char am_pm;
	char brightness;
};

enum st_lpm_pio_level {
/* Interrupt/Power off  will be generated when bit goes 1 to 0*/
	ST_LPM_PIO_LOW,
/* Interrupt/Power off  will be generated when bit goes 0 to 1*/
	ST_LPM_PIO_HIGH
};

enum st_lpm_pio_direction {
	ST_LPM_PIO_INPUT,
	ST_LPM_PIO_OUTPUT
};

/**
 * enum st_lpm_pio_use - to define how pio can be used
 * @ST_LPM_PIO_POWER:		PIO used for power control
 * @ST_LPM_PIO_ETH_MDINT:	PIO used for phy WOL
 * @ST_LPM_PIO_WAKEUP:		PIO used as GPIO interrupt for wakeup
 * @ST_LPM_PIO_EXT_IT:		PIO used as external interrupt
 * @ST_LPM_PIO_OTHER:		Reserved
 */
enum st_lpm_pio_use {
	ST_LPM_PIO_POWER = 1,
	ST_LPM_PIO_ETH_MDINT,
	ST_LPM_PIO_WAKEUP,
	ST_LPM_PIO_EXT_IT,
	ST_LPM_PIO_OTHER,
	ST_LPM_PIO_FP_PIO,
};

/**
 * struct st_lpm_pio_setting - define PIO use
 * @pio_bank:		pio bank number
 * @pio_pin:		pio pin number, valid values [0-7]
 * @pio_direction:	direction of PIO
 *			0 means, pio is used as input.
 *			1 means, pio is used as output.
 * @interrupt_enabled:	If interrupt on this PIO is enabled
 *			0 means, interrupts are disabled.
 *			1 means, interrupt are enabled.
 *			This must be set to 0 when pio is used as output.
 * @pio_level:		PIO level high or low.
 *			0 means, Interrupt/Power off will be done when
 *			PIO goes low.
 *			1 means, Interrupt/Power off will be done when
 *			PIO goes high.
 * @pio_use:		use of this pio
 *
 */
struct st_lpm_pio_setting {
	char pio_bank;
	char pio_pin;
	bool pio_direction;
	bool interrupt_enabled;
	bool pio_level;
	enum st_lpm_pio_use  pio_use;
};

/**
 * enum st_lpm_adv_feature_name - Define name of advance feature of SBC
 * @ST_LPM_USE_EXT_VCORE:	feature is external VCORE for SBC
 * @ST_LPM_USE_INT_VOLT_DETECT:	internal low voltage detect
 * @ST_LPM_EXT_CLOCK:		external clock
 * @ST_LPM_RTC_SOURCE:		RTC source for SBC
 * @ST_LPM_WU_TRIGGERS:		wakeup triggers
 * @ST_LPM_DE_BOUNCE:		Delay for power de bounce
 * @ST_LPM_DC_STABLE:		Delay need for DC stability
 */
enum st_lpm_adv_feature_name {
	ST_LPM_USE_EXT_VCORE = 1,
	ST_LPM_USE_INT_VOLT_DETECT,
	ST_LPM_EXT_CLOCK,
	ST_LPM_RTC_SOURCE,
	ST_LPM_WU_TRIGGERS,
	ST_LPM_DE_BOUNCE,
	ST_LPM_DC_STABLE,
	ST_LPM_SBC_IDLE,
	ST_LPM_RTC_CALIBRATION_TIME,
	ST_LPM_SET_EXIT_CPS,
	ST_LPM_SBC_USER_CUSTOM_MSG = 0x40,
};

/**
 * struct st_lpm_adv_feature - define advance feature struct
 * @feature_name:	Name of feature
 * @set_params:	Used to set feature on SBC
 *
 *		when features is ST_LPM_USE_EXT_VCORE,
 *		set_params[0] = 0 means External Vcore not selected
 *		set_params[0] = 1 means External Vcore selected
 *		set_params[1] is unused
 *
 *		when features is ST_LPM_USE_INT_VOLT_DETECT
 *		set_params[0] is voltage value to detect low voltage
 *		i..e for 3.3V use 33 and for 3.0V use 20
 *		set_params[1] is unused
 *
 *		when features is ST_LPM_EXT_CLOCK
 *		set_params[0] = 1 means external
 *		set_params[0] = 2 means external ACG
 *		set_params[0] = 3 means Track_32K
 *		set_params[1] is unused
 *
 *		when features is ST_LPM_RTC_SOURCE
 *		set_params[0] = 1 means RTC_32K_TCXO
 *		set_params[0] = 2 means external ACG
 *		set_params[0] = 3 means RTC_32K_OSC
 *		set_params[1] is unused
 *
 *		when features is ST_LPM_WU_TRIGGERS
 *		set_params[0-1] is bit map for each wakeup trigger
 *		as defined in enum st_lpm_wakeup_devices
 *
 *		when features is ST_LPM_DE_BOUNCE
 *		set_params[0-1] is debounce delay in ms
 *
 *		when features is ST_LPM_DC_STABLE
 *		set_params[0-1] is delay in ms for DC stability
 *
 * @get_params: Used to get advance feature of SBC
 *		get_params[0-3] is feature set supported by SBC, TBD
 *		get_params[4-5] is wakeup triggers
 */
struct st_lpm_adv_feature {
	enum st_lpm_adv_feature_name feature_name;
	union {
		unsigned char set_params[12];
		unsigned char get_params[10];
	} params;
};

/* defines  MAX depth for IR FIFO */
#define MAX_IR_FIFO_DEPTH	64

#define MIN_RTC_YEAR 2000
#define MAX_RTC_YEAR 2255

/**
 * struct st_lpm_ir_fifo - define one element of IR fifo
 * @mark:	Mark time
 * @symbol:	Symbol time
 */
struct st_lpm_ir_fifo {
	u16 mark;
	u16 symbol;
};

/**
 * sturct st_lpm_ir_key - define raw data for IR key
 * @key_index:		Key Index acts as key identifier
 * @num_patterns:	Number of mark/symbol pattern define this key
 * @fifo:		Place holder for mark/symbol data
 *
 * Max value of fifo is kept 64, which is max value of IR IP
 */
struct st_lpm_ir_key {
	u8 key_index;
	u8 num_patterns;
	struct st_lpm_ir_fifo fifo[MAX_IR_FIFO_DEPTH];
};

/**
 * struct st_lpm_ir_keyinfo - define a IR key along with another info
 * @ir_id:	Id of IR hardware, use id 0 for first IR, 1 for second IR
 *		use id 0x80 for first UHF and 0x81 for second so on
 * @time_period:Time period for this key , this is dependent on protocol
 * @time_out:	Time out period for this key
 * @tolerance:	Expected tolerance in IR key from standard value
 * @ir_key:	IR key data
 */
struct st_lpm_ir_keyinfo {
	u8 ir_id;
	u16 time_period;
	u16 time_out;
	u8 tolerance;
	struct st_lpm_ir_key ir_key;
};

/**
 * struct st_lpm_cec_address - define CEC address
 * @phy_addr :	physical address
 *		phy_addr is U16 which are represented as a.b.c.d
 *		here in a.b.c.d each a.b.c or d is > 0 and <= f
 *		So pack these four character into U16 before sending
 *		1.0.0.0 means send 1, 1.3.0.0 mean send 0x31
 * @logical_addr:	logical address
 *			This is bit field, for each 15 address
 */
struct st_lpm_cec_address {
	u16 phy_addr;
	u16 logical_addr;
};

/**
 * enum st_lpm_cec_select
 * @ST_LPM_CONFIG_CEC_WU_REASON: Enabled disable CEC WU reason
 * @ST_LPM_CONFIG_CEC_WU_CUSTOM_MSG Set Custom message for CEC WU
 */
enum st_lpm_cec_select {
	ST_LPM_CONFIG_CEC_WU_REASON = 1,
	ST_LPM_CONFIG_CEC_WU_CUSTOM_MSG,
	ST_LPM_CONFIG_CEC_VERSION,
	ST_LPM_CONFIG_CEC_DEVICE_VENDOR_ID,
};

/**
 * enum st_lpm_cec_wu_reason - Define CEC WU reason
 * @ST_LPM_CEC_WU_STREAM_PATH : opcode 0x86
 * @ST_LPM_CEC_WU_USER_POWER  : opcode 0x44, oprand 0x40
 * @ST_LPM_CEC_WU_STREAM_POWER_ON : opcode 0x44, oprand 0x6B
 * @ST_LPM_CEC_WU_STREAM_POWER_TOGGLE : opcode 0x44, oprand 0x6D
 * @ST_LPM_CEC_WU_USER_MSG : user defined message
 */
enum st_lpm_cec_wu_reason {
	ST_LPM_CEC_WU_STREAM_PATH		= BIT(0),
	ST_LPM_CEC_WU_USER_POWER		= BIT(1),
	ST_LPM_CEC_WU_STREAM_POWER_ON		= BIT(2),
	ST_LPM_CEC_WU_STREAM_POWER_TOGGLE	= BIT(3),
	ST_LPM_CEC_WU_USER_MSG			= BIT(4),
};

/**
 * enum st_lpm_sbc_dmem_offset_type - Define offsets inside SBC-DMEM
 * @ST_SBC_DMEM_DTU : offset for DTU buffer
 * @ST_SBC_DMEM_CPS_TAG : offset for CPS Tag
 * @ST_SBC_DMEM_PEN_HOLDING_VAR : offset for pen holding variable
 */
enum st_lpm_sbc_dmem_offset_type {
	ST_SBC_DMEM_DTU,
	ST_SBC_DMEM_CPS_TAG,
	ST_SBC_DMEM_PEN_HOLDING_VAR,
};

/**
 * struct st_lpm_cec_custom_msg - Define CEC custom message
 * @size : size of message
 * @opcode : opcode
 * @oprand : oprand
 */
struct st_lpm_cec_custom_msg {
	u8 size;
	u8 opcode;
	u8 oprand[10];
};

enum st_lpm_inform_host_event {
	ST_LPM_LONG_PIO_PRESS = 1,
	ST_LPM_ALARM_TIMER
};

#define ST_LPM_CEC_MAX_OSD_NAME_LENGTH	14

struct st_lpm_cec_osd_msg {
	u8 size;
	u8 name[ST_LPM_CEC_MAX_OSD_NAME_LENGTH];
};

#define ST_LPM_EDID_BLK_SIZE		128
#define ST_LPM_EDID_MAX_BLK_NUM		3
#define ST_LPM_EDID_BLK_END		0xFF

/**
 * union st_lpm_cec_params - to define CEC params
 * @cec_wu_reasons :	Bit field for each CEC wakeup device
 *			Bits should be set as per enum st_lpm_cec_wu_reason
 * @cec_msg :		user defined CEC message
 */
union st_lpm_cec_params {
	u8 cec_wu_reasons;
	struct st_lpm_cec_custom_msg cec_msg;
};

int st_lpm_configure_wdt(u16 time_in_ms, u8 wdt_type);

int st_lpm_get_fw_state(enum st_lpm_sbc_state *fw_state);

int st_lpm_get_wakeup_device(enum st_lpm_wakeup_devices *wakeupdevice);

int st_lpm_get_trigger_data(enum st_lpm_wakeup_devices wakeup_device,
		unsigned int size_max, char *data);

int st_lpm_get_wakeup_info(enum st_lpm_wakeup_devices *wakeupdevice,
	s16 *validsize, u16 datasize, char  *data);

int st_lpm_write_edid(u8 *data, u8 block_num);

int st_lpm_read_edid(u8 *data, u8 block_num);

int st_lpm_write_dmem(unsigned char *data, unsigned int size,
		      int offset);

int st_lpm_read_dmem(unsigned char *data, unsigned int size,
		     int offset);

int st_lpm_get_dmem_offset(enum st_lpm_sbc_dmem_offset_type offset_type);

int st_lpm_get_version(struct st_lpm_version *drv_ver,
	struct st_lpm_version *fw_ver);

int st_lpm_reset(enum st_lpm_reset_type reset_type);

int st_lpm_setup_fp(struct st_lpm_fp_setting *fp_setting);

int st_lpm_setup_ir(u8 num_keys, struct st_lpm_ir_keyinfo *keys);

int st_lpm_set_rtc(struct rtc_time *new_rtc);

int st_lpm_get_rtc(struct rtc_time *new_rtc);

int st_lpm_set_wakeup_device(u16  wakeup_devices);

int st_lpm_set_wakeup_time(u32 timeout);

int st_lpm_setup_pio(struct st_lpm_pio_setting *pio_setting);

int st_lpm_setup_keyscan(u16 key_data);

int st_lpm_set_adv_feature(u8 enabled, struct st_lpm_adv_feature *feature);

int st_lpm_get_adv_feature(bool all_features, bool custom_feature,
				struct st_lpm_adv_feature *feature);

enum st_lpm_callback_type {
	ST_LPM_FP_PIO_PRESS,
	ST_LPM_RTC,
	ST_LPM_GPIO_WAKEUP,
	ST_LPM_MAX, /* to mark the max allowed */
};

/* to register user reset function */
int st_lpm_register_callback(enum st_lpm_callback_type type,
	int (*func)(void *), void *data);

int st_lpm_notify(enum st_lpm_callback_type type);

int st_lpm_reload_fw_prepare(void);

int st_start_loaded_fw(void);

/**
 * function - st_lpm_setup_fp_pio
 * function to inform SBC about FP PIO
 * This PIO is used to detect FP PIO Long press as defined in
 * long_press_delay in ms.
 * After detecting GP PIO long press SBC will send message to
 * Host to invoke call back st_lpm_reset_notifier_fn.
 * If no reply from host then SBC will reset the SOC after
 * delay specified in default_reset_delay ms.
 * if st_lpm_reset_notifier_fn specify some other time then
 * SBC will reset SOC after that delay ms.
 */
int st_lpm_setup_fp_pio(struct st_lpm_pio_setting *pio_setting,
			u32 long_press_delay, u32 default_reset_delay);

int st_lpm_setup_power_on_delay(u16 de_bounce_delay, u16 dc_stability_delay);

int st_lpm_setup_rtc_calibration_time(u8 cal_time);

int st_lpm_set_cec_addr(struct st_lpm_cec_address *addr);

int st_lpm_cec_set_osd_name(struct st_lpm_cec_osd_msg *params);

int st_lpm_cec_config(enum st_lpm_cec_select use,
			union st_lpm_cec_params *params);

int st_lpm_get_rtc(struct rtc_time *new_rtc);

/*
 * Maximum size of message data that can be send in Not Bulk mode,
 */
#define LPM_MAX_MSG_DATA	14

int st_lpm_get_command_data_size(unsigned char command_id);

/**
 * enum st_lpm_config_reboot_type - Define DDR state while booting
 * @ST_LPM_REBOOT_WITH_DDR_SELF_REFRESH
 * @ST_LPM_REBOOT_WITH_DDR_OFF
 */
enum st_lpm_config_reboot_type {
	ST_LPM_REBOOT_WITH_DDR_SELF_REFRESH,
	ST_LPM_REBOOT_WITH_DDR_OFF,
};

int st_lpm_config_reboot(enum st_lpm_config_reboot_type type);

int st_lpm_sbc_ir_enable(bool enable);

int st_lpm_poweroff(void);

/**
 * lpm_message - LPM message for cross CPU communication
 * @command_id: Command ID
 * @transaction_id:     Transaction id
 * @msg_data:   Message data associated with this command
 *
 * Normally each message is less than 16 bytes
 * Any message more than 16 bytes considered as big message
 *
 * In case of internal SBC, where size of mailbox is 16 byte
 * If message is large then 16 bytes then direct write to SBC memory is done
 *
 */

struct lpm_message {
	unsigned char command_id;
	unsigned char transaction_id;
	unsigned char buf[LPM_MAX_MSG_DATA];
} __packed;

struct st_lpm_ops {
	int (*exchange_msg)(struct lpm_message *cmd,
		struct lpm_message *response, void *private_data);
	int (*write_bulk)(u16 size, const char *msg, void *private_data,
			  int *offset_dmem);
	int (*read_bulk)(u16 size, u16 offset, char *msg, void *private_data);
	int (*get_dmem_offset)(enum st_lpm_sbc_dmem_offset_type offset_type,
			       void *private_data);
	int (*config_reboot)(enum st_lpm_config_reboot_type type,
		void *private_data);
	void (*ir_enable)(bool enable, void *private_data);

	int (*write_edid)(u8 *edid_buf, u8 block_num, void *data);

	int (*read_edid)(u8 *edid_buf, u8 block_num, void *data);

	int (*reload_fw_prepare)(void *data);

	int (*start_loaded_fw)(void *data);
};

int st_lpm_set_ops(struct st_lpm_ops *ops, void *private_data);

#endif /* __LPM_H */
